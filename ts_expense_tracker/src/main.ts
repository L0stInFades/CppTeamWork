import * as readline from "node:readline";
import * as fs from "node:fs";

const MAX_EXPENSES = 1000;
const MAX_UNIQUE_CATEGORIES_PER_MONTH = 20;
const DATA_FILE = "expenses.dat";
const SETTLEMENT_FILE = "settlement_status.txt";
const MAX_DESCRIPTION_LENGTH = 100;
const MAX_CATEGORY_LENGTH = 50;

class Expense {
    year = 0;
    month = 0;
    day = 0;
    description = "";
    amount = 0.0;
    category = "";

    setData(y: number, m: number, d: number, desc: string, amt: number, cat: string): void {
        this.year = y;
        this.month = m;
        this.day = d;
        this.description = desc;
        this.amount = amt;
        this.category = cat;
    }
}

interface CategorySum {
    name: string;
    total: number;
}

// ── Helpers ──

function pad2(n: number): string {
    return n.toString().padStart(2, "0");
}

function padRight(s: string, w: number): string {
    if (s.length >= w) return s;
    return s + " ".repeat(w - s.length);
}

function padLeft(s: string, w: number): string {
    if (s.length >= w) return s;
    return " ".repeat(w - s.length) + s;
}

function fmtAmount(n: number): string {
    return n.toFixed(2);
}

function repeatChar(ch: string, n: number): string {
    return ch.repeat(n);
}

function printExpenseHeader(): void {
    process.stdout.write(
        `${padRight("日期", 12)}${padRight("描述", 30)}${padRight("类别", 20)}${padLeft("金额", 10)}\n`
    );
    process.stdout.write(repeatChar("-", 72) + "\n");
}

function printExpenseHeaderWithIndex(): void {
    process.stdout.write(
        `${padRight("序号", 5)}${padRight("日期", 12)}${padRight("描述", 30)}${padRight("类别", 20)}${padLeft("金额", 10)}\n`
    );
    process.stdout.write(repeatChar("-", 77) + "\n");
}

function formatDateStr(exp: Expense): string {
    return `${exp.year}-${pad2(exp.month)}-${pad2(exp.day)}`;
}

function printExpenseRow(exp: Expense): void {
    const date = formatDateStr(exp);
    process.stdout.write(
        `${padRight(date, 12)}${padRight(exp.description, 30)}${padRight(exp.category, 20)}${padLeft(fmtAmount(exp.amount), 10)}\n`
    );
}

function printExpenseRowWithIndex(index: number, exp: Expense): void {
    const date = formatDateStr(exp);
    process.stdout.write(
        `${padRight(index.toString(), 5)}${padRight(date, 12)}${padRight(exp.description, 30)}${padRight(exp.category, 20)}${padLeft(fmtAmount(exp.amount), 10)}\n`
    );
}

// ── Readline wrapper ──

class LineReader {
    private rl: readline.Interface;
    private lines: string[] = [];
    private waiting: ((line: string) => void) | null = null;
    private closed = false;

    constructor() {
        this.rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            terminal: false,
        });
        this.rl.on("line", (line: string) => {
            if (this.waiting) {
                const cb = this.waiting;
                this.waiting = null;
                cb(line);
            } else {
                this.lines.push(line);
            }
        });
        this.rl.on("close", () => {
            this.closed = true;
            if (this.waiting) {
                const cb = this.waiting;
                this.waiting = null;
                cb("");
            }
        });
    }

    readLine(): Promise<string> {
        if (this.lines.length > 0) {
            return Promise.resolve(this.lines.shift()!);
        }
        if (this.closed) {
            return Promise.resolve("");
        }
        return new Promise<string>((resolve) => {
            this.waiting = resolve;
        });
    }

    async readInt(): Promise<{ value: number; ok: boolean }> {
        const line = await this.readLine();
        const trimmed = line.trim();
        const n = parseInt(trimmed, 10);
        if (isNaN(n) || trimmed === "") {
            return { value: 0, ok: false };
        }
        // Make sure the entire string is a valid integer
        if (n.toString() !== trimmed && ("+" + n.toString()) !== trimmed) {
            // Allow things like "  123  " but not "12abc"
            if (!/^[+-]?\d+$/.test(trimmed)) {
                return { value: 0, ok: false };
            }
        }
        return { value: n, ok: true };
    }

    close(): void {
        this.rl.close();
    }
}

function write(s: string): void {
    process.stdout.write(s);
}

function writeln(s: string): void {
    process.stdout.write(s + "\n");
}

// ── ExpenseTracker ──

class ExpenseTracker {
    private allExpenses: Expense[] = [];
    private expenseCount = 0;
    private reader: LineReader;

    constructor(reader: LineReader) {
        this.reader = reader;
        for (let i = 0; i < MAX_EXPENSES; i++) {
            this.allExpenses.push(new Expense());
        }
    }

    async init(): Promise<void> {
        if (this.loadExpenses()) {
            writeln(`成功加载 ${this.expenseCount} 条历史记录。`);
        } else {
            writeln("未找到历史数据文件或加载失败，开始新的记录。");
        }
        this.performAutomaticSettlement();
    }

    async run(): Promise<void> {
        while (true) {
            writeln("\n大学生开销追踪器");
            writeln("--------------------");
            writeln("1. 添加开销记录");
            writeln("2. 查看所有开销");
            writeln("3. 查看月度统计");
            writeln("4. 按期间列出开销");
            writeln("5. 删除开销记录");
            writeln("6. 保存并退出");
            writeln("--------------------");
            write("请输入选项: ");

            const { value: choice } = await this.reader.readInt();

            switch (choice) {
                case 1:
                    await this.addExpense();
                    break;
                case 2:
                    this.displayAllExpenses();
                    break;
                case 3:
                    await this.displayMonthlySummary();
                    break;
                case 4:
                    await this.listExpensesByPeriod();
                    break;
                case 5:
                    await this.deleteExpense();
                    break;
                case 6:
                    this.saveExpenses();
                    writeln("数据已保存。正在退出...");
                    return;
                default:
                    writeln("无效选项，请重试。");
                    break;
            }
        }
    }

    private async addExpense(): Promise<void> {
        if (this.expenseCount >= MAX_EXPENSES) {
            writeln("错误：开销记录已满！无法添加更多记录。");
            return;
        }

        writeln("\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---");

        const now = new Date();
        const currentYear = now.getFullYear();
        const currentMonth = now.getMonth() + 1;
        const currentDay = now.getDate();

        // Get year
        write(`输入年份 (YYYY) [默认: ${currentYear}, -1 取消]: `);
        let lineInput = await this.reader.readLine();
        if (lineInput === "-1") { writeln("已取消添加开销。"); return; }
        let year = currentYear;
        if (lineInput.trim() !== "") {
            const n = parseInt(lineInput.trim(), 10);
            if (!isNaN(n) && /^-?\d+$/.test(lineInput.trim())) {
                year = n;
            } else {
                writeln(`年份输入无效或包含非数字字符，将使用默认年份: ${currentYear}。`);
            }
        }

        // Get month
        write(`输入月份 (MM) [默认: ${currentMonth}, -1 取消]: `);
        lineInput = await this.reader.readLine();
        if (lineInput === "-1") { writeln("已取消添加开销。"); return; }
        let month = currentMonth;
        if (lineInput.trim() !== "") {
            const n = parseInt(lineInput.trim(), 10);
            if (!isNaN(n) && n >= 1 && n <= 12 && /^-?\d+$/.test(lineInput.trim())) {
                month = n;
            } else {
                writeln(`月份输入无效或范围不正确 (1-12)，将使用默认月份: ${currentMonth}。`);
            }
        }

        // Get day
        write(`输入日期 (DD) [默认: ${currentDay}, -1 取消]: `);
        lineInput = await this.reader.readLine();
        if (lineInput === "-1") { writeln("已取消添加开销。"); return; }
        let day = currentDay;
        if (lineInput.trim() !== "") {
            const n = parseInt(lineInput.trim(), 10);
            if (!isNaN(n) && n >= 1 && n <= 31 && /^-?\d+$/.test(lineInput.trim())) {
                day = n;
            } else {
                writeln(`日期输入无效或范围不正确 (1-31)，将使用默认日期: ${currentDay}。`);
            }
        }

        // Basic date validation
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            writeln("日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。");
            return;
        }

        // Get description
        write(`输入描述 (最多 ${MAX_DESCRIPTION_LENGTH} 字符, 输入 '!cancel' 取消): `);
        let description = await this.reader.readLine();
        if (description === "!cancel") { writeln("已取消添加开销。"); return; }
        if (description.length > MAX_DESCRIPTION_LENGTH) {
            writeln(`描述过长，已截断为 ${MAX_DESCRIPTION_LENGTH} 字符。`);
            description = description.substring(0, MAX_DESCRIPTION_LENGTH);
        }

        // Get amount
        write("输入金额 (-1 取消): ");
        let amount = 0;
        while (true) {
            lineInput = await this.reader.readLine();
            if (lineInput === "-1") { writeln("已取消添加开销。"); return; }
            const a = parseFloat(lineInput.trim());
            if (!isNaN(a) && a >= 0 && /^[+-]?(\d+\.?\d*|\.\d+)$/.test(lineInput.trim())) {
                amount = a;
                break;
            }
            write("金额无效或为负，请重新输入 (-1 取消): ");
        }

        // Get category
        write(`输入类别 (如 餐饮, 交通, 娱乐; 最多 ${MAX_CATEGORY_LENGTH} 字符, 输入 '!cancel' 取消): `);
        let category = await this.reader.readLine();
        if (category === "!cancel") { writeln("已取消添加开销。"); return; }
        if (category.length > MAX_CATEGORY_LENGTH) {
            writeln(`类别名称过长，已截断为 ${MAX_CATEGORY_LENGTH} 字符。`);
            category = category.substring(0, MAX_CATEGORY_LENGTH);
        }
        if (category === "") {
            category = "未分类";
        }

        this.allExpenses[this.expenseCount]!.setData(year, month, day, description, amount, category);
        this.expenseCount++;
        writeln("开销已添加。");
    }

    private displayAllExpenses(): void {
        if (this.expenseCount === 0) {
            writeln("没有开销记录。");
            return;
        }
        writeln("\n--- 所有开销记录 ---");
        printExpenseHeader();
        for (let i = 0; i < this.expenseCount; i++) {
            printExpenseRow(this.allExpenses[i]!);
        }
        writeln(repeatChar("-", 72));
    }

    private async displayMonthlySummary(): Promise<void> {
        writeln("\n--- 月度开销统计 ---");

        // Get year
        write("输入要统计的年份 (YYYY) (-1 取消): ");
        let year = 0;
        while (true) {
            const { value, ok } = await this.reader.readInt();
            if (ok) {
                if (value === -1) { writeln("已取消月度统计。"); return; }
                year = value;
                break;
            }
            write("年份输入无效，请重新输入 (-1 取消): ");
        }

        // Get month
        write("输入要统计的月份 (MM) (-1 取消): ");
        let month = 0;
        while (true) {
            const { value, ok } = await this.reader.readInt();
            if (ok) {
                if (value === -1) { writeln("已取消月度统计。"); return; }
                if (value >= 1 && value <= 12) {
                    month = value;
                    break;
                }
                write("月份输入无效 (1-12)，请重新输入 (-1 取消): ");
            } else {
                write("月份输入无效 (1-12)，请重新输入 (-1 取消): ");
            }
        }

        writeln(`\n--- ${year}年${pad2(month)}月 开销统计 ---`);

        let totalMonthAmount = 0;
        let foundRecords = false;
        const categorySums: CategorySum[] = [];

        printExpenseHeader();

        for (let i = 0; i < this.expenseCount; i++) {
            const exp = this.allExpenses[i]!;
            if (exp.year === year && exp.month === month) {
                foundRecords = true;
                printExpenseRow(exp);
                totalMonthAmount += exp.amount;

                let categoryExists = false;
                for (const cs of categorySums) {
                    if (cs.name === exp.category) {
                        cs.total += exp.amount;
                        categoryExists = true;
                        break;
                    }
                }
                if (!categoryExists && categorySums.length < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
                    categorySums.push({ name: exp.category, total: exp.amount });
                }
            }
        }

        if (!foundRecords) {
            writeln("该月份没有开销记录。");
        } else {
            writeln(repeatChar("-", 72));
            writeln(`${padRight("本月总计:", 62)}${padLeft(fmtAmount(totalMonthAmount), 10)}`);
            writeln("");

            if (categorySums.length > 0) {
                writeln("按类别汇总:");
                writeln(`${padRight("类别", 20)}${padLeft("总金额", 10)}`);
                writeln(repeatChar("-", 30));
                for (const cs of categorySums) {
                    writeln(`${padRight(cs.name, 20)}${padLeft(fmtAmount(cs.total), 10)}`);
                }
                writeln(repeatChar("-", 30));
            }
        }
    }

    private async listExpensesByPeriod(): Promise<void> {
        while (true) {
            writeln("\n--- 按期间列出开销 --- ");
            writeln("1. 按年份列出");
            writeln("2. 按月份列出");
            writeln("3. 按日期列出");
            writeln("0. 返回主菜单");
            writeln("--------------------");
            write("请输入选项: ");

            const { value: choice } = await this.reader.readInt();

            switch (choice) {
                case 1: {
                    writeln("\n--- 按年份列出开销 ---");
                    write("输入年份 (YYYY) (输入 0 返回): ");
                    let year = 0;
                    while (true) {
                        const { value, ok } = await this.reader.readInt();
                        if (ok) { year = value; break; }
                        write("年份输入无效，请重新输入 (输入 0 返回): ");
                    }
                    if (year === 0) continue;

                    let found = false;
                    printExpenseHeader();
                    for (let i = 0; i < this.expenseCount; i++) {
                        if (this.allExpenses[i]!.year === year) {
                            printExpenseRow(this.allExpenses[i]!);
                            found = true;
                        }
                    }
                    if (!found) writeln(`在 ${year} 年没有找到开销记录。`);
                    writeln(repeatChar("-", 72));
                    break;
                }
                case 2: {
                    writeln("\n--- 按月份列出开销 ---");
                    write("输入年份 (YYYY) (输入 0 返回): ");
                    let year = 0;
                    while (true) {
                        const { value, ok } = await this.reader.readInt();
                        if (ok) { year = value; break; }
                        write("年份输入无效，请重新输入 (输入 0 返回): ");
                    }
                    if (year === 0) continue;

                    write("输入月份 (MM) (输入 0 返回): ");
                    let month = 0;
                    while (true) {
                        const { value, ok } = await this.reader.readInt();
                        if (ok && (value === 0 || (value >= 1 && value <= 12))) {
                            month = value; break;
                        }
                        write("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                    }
                    if (month === 0) continue;

                    let found = false;
                    printExpenseHeader();
                    for (let i = 0; i < this.expenseCount; i++) {
                        const exp = this.allExpenses[i]!;
                        if (exp.year === year && exp.month === month) {
                            printExpenseRow(exp);
                            found = true;
                        }
                    }
                    if (!found) writeln(`在 ${year} 年 ${month} 月没有找到开销记录。`);
                    writeln(repeatChar("-", 72));
                    break;
                }
                case 3: {
                    writeln("\n--- 按日期列出开销 ---");
                    write("输入年份 (YYYY) (输入 0 返回): ");
                    let year = 0;
                    while (true) {
                        const { value, ok } = await this.reader.readInt();
                        if (ok) { year = value; break; }
                        write("年份输入无效，请重新输入 (输入 0 返回): ");
                    }
                    if (year === 0) continue;

                    write("输入月份 (MM) (输入 0 返回): ");
                    let month = 0;
                    while (true) {
                        const { value, ok } = await this.reader.readInt();
                        if (ok && (value === 0 || (value >= 1 && value <= 12))) {
                            month = value; break;
                        }
                        write("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                    }
                    if (month === 0) continue;

                    write("输入日期 (DD) (输入 0 返回): ");
                    let day = 0;
                    while (true) {
                        const { value, ok } = await this.reader.readInt();
                        if (ok && (value === 0 || (value >= 1 && value <= 31))) {
                            day = value; break;
                        }
                        write("日期输入无效 (1-31)，请重新输入 (输入 0 返回): ");
                    }
                    if (day === 0) continue;

                    let found = false;
                    printExpenseHeader();
                    for (let i = 0; i < this.expenseCount; i++) {
                        const exp = this.allExpenses[i]!;
                        if (exp.year === year && exp.month === month && exp.day === day) {
                            printExpenseRow(exp);
                            found = true;
                        }
                    }
                    if (!found) writeln(`在 ${year} 年 ${month} 月 ${day} 日没有找到开销记录。`);
                    writeln(repeatChar("-", 72));
                    break;
                }
                case 0:
                    writeln("返回主菜单...");
                    return;
                default:
                    writeln("无效选项，请重试。");
                    break;
            }
        }
    }

    private saveExpenses(): void {
        let content = `${this.expenseCount}\n`;
        for (let i = 0; i < this.expenseCount; i++) {
            const exp = this.allExpenses[i]!;
            content += `${exp.year},${exp.month},${exp.day},${exp.description},${exp.amount},${exp.category}\n`;
        }
        try {
            fs.writeFileSync(DATA_FILE, content, "utf-8");
        } catch {
            process.stderr.write(`错误：无法打开文件 ${DATA_FILE} 进行写入！\n`);
        }
    }

    private loadExpenses(): boolean {
        let content: string;
        try {
            content = fs.readFileSync(DATA_FILE, "utf-8");
        } catch {
            return false;
        }

        const lines = content.trimEnd().split("\n");
        if (lines.length === 0) {
            this.expenseCount = 0;
            return false;
        }

        const countFromFile = parseInt(lines[0]!.trim(), 10);
        if (isNaN(countFromFile) || countFromFile < 0 || countFromFile > MAX_EXPENSES) {
            this.expenseCount = 0;
            return false;
        }

        let loadedCount = 0;
        for (let i = 1; i < lines.length && i - 1 < countFromFile; i++) {
            if (loadedCount >= MAX_EXPENSES) break;

            const line = lines[i]!;
            const parts = splitN(line, ",", 6);

            // Parse year
            if (parts.length < 1) {
                process.stderr.write(`警告：记录 ${i} 数据不完整 (年份)。\n`);
                continue;
            }
            const year = parseInt(parts[0]!, 10);
            if (isNaN(year)) {
                process.stderr.write(`警告：无效年份格式 '${parts[0]}' 在记录 ${i}。跳过此记录。\n`);
                continue;
            }

            // Parse month
            if (parts.length < 2) {
                process.stderr.write(`警告：记录 ${i} 数据不完整 (月份)。\n`);
                continue;
            }
            const month = parseInt(parts[1]!, 10);
            if (isNaN(month)) {
                process.stderr.write(`警告：无效月份格式 '${parts[1]}' 在记录 ${i}。跳过此记录。\n`);
                continue;
            }

            // Parse day
            if (parts.length < 3) {
                process.stderr.write(`警告：记录 ${i} 数据不完整 (日期)。\n`);
                continue;
            }
            const day = parseInt(parts[2]!, 10);
            if (isNaN(day)) {
                process.stderr.write(`警告：无效日期格式 '${parts[2]}' 在记录 ${i}。跳过此记录。\n`);
                continue;
            }

            // Parse description
            if (parts.length < 4) {
                process.stderr.write(`警告：记录 ${i} 数据不完整 (描述)。\n`);
                continue;
            }
            let descriptionStr = parts[3]!;
            if (descriptionStr.length > MAX_DESCRIPTION_LENGTH) {
                descriptionStr = descriptionStr.substring(0, MAX_DESCRIPTION_LENGTH);
            }

            // Parse amount
            if (parts.length < 5) {
                process.stderr.write(`警告：记录 ${i} 数据不完整 (金额)。\n`);
                continue;
            }
            const amount = parseFloat(parts[4]!);
            if (isNaN(amount)) {
                process.stderr.write(`警告：无效金额格式 '${parts[4]}' 在记录 ${i}。跳过此记录。\n`);
                continue;
            }

            // Parse category
            let categoryStr = "";
            if (parts.length >= 6) {
                categoryStr = parts[5]!;
                if (categoryStr.length > MAX_CATEGORY_LENGTH) {
                    categoryStr = categoryStr.substring(0, MAX_CATEGORY_LENGTH);
                }
            }

            this.allExpenses[loadedCount]!.setData(year, month, day, descriptionStr, amount, categoryStr);
            loadedCount++;
        }

        this.expenseCount = loadedCount;
        return true;
    }

    private readLastSettlement(): [number, number] {
        let lastYear = 0;
        let lastMonth = 0;
        try {
            const content = fs.readFileSync(SETTLEMENT_FILE, "utf-8");
            const parts = content.trim().split(/\s+/);
            if (parts[0]) lastYear = parseInt(parts[0], 10) || 0;
            if (parts[1]) lastMonth = parseInt(parts[1], 10) || 0;
        } catch {
            // File doesn't exist
        }
        return [lastYear, lastMonth];
    }

    private writeLastSettlement(year: number, month: number): void {
        try {
            fs.writeFileSync(SETTLEMENT_FILE, `${year} ${month}\n`, "utf-8");
        } catch {
            process.stderr.write(`错误：无法写入结算状态文件 ${SETTLEMENT_FILE}\n`);
        }
    }

    private generateMonthlyReportForSettlement(year: number, month: number): void {
        writeln(`\n--- ${year}年${pad2(month)}月 开销报告 (自动结算) ---`);

        let totalMonthAmount = 0;
        let foundRecords = false;
        const categorySums: CategorySum[] = [];

        writeln("明细:");
        printExpenseHeader();

        for (let i = 0; i < this.expenseCount; i++) {
            const exp = this.allExpenses[i]!;
            if (exp.year === year && exp.month === month) {
                foundRecords = true;
                printExpenseRow(exp);
                totalMonthAmount += exp.amount;

                let categoryExists = false;
                for (const cs of categorySums) {
                    if (cs.name === exp.category) {
                        cs.total += exp.amount;
                        categoryExists = true;
                        break;
                    }
                }
                if (!categoryExists && categorySums.length < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
                    categorySums.push({ name: exp.category, total: exp.amount });
                }
            }
        }

        if (!foundRecords) {
            writeln("该月份没有开销记录。");
            return;
        }

        writeln(repeatChar("-", 72));
        writeln(`${padRight("本月总计:", 62)}${padLeft(fmtAmount(totalMonthAmount), 10)}`);
        writeln("");

        if (categorySums.length > 0) {
            writeln("按类别汇总:");
            writeln(`${padRight("类别", 20)}${padLeft("总金额", 10)}`);
            writeln(repeatChar("-", 30));
            for (const cs of categorySums) {
                writeln(`${padRight(cs.name, 20)}${padLeft(fmtAmount(cs.total), 10)}`);
            }
            writeln(repeatChar("-", 30));
        }

        writeln("--- 报告生成完毕 ---");
    }

    private performAutomaticSettlement(): void {
        let [lastSettledYear, lastSettledMonth] = this.readLastSettlement();

        const now = new Date();
        const currentYear = now.getFullYear();
        const currentMonth = now.getMonth() + 1;

        if (lastSettledYear === 0) {
            lastSettledYear = currentYear;
            lastSettledMonth = currentMonth;
            if (lastSettledMonth === 1) {
                lastSettledMonth = 12;
                lastSettledYear--;
            } else {
                lastSettledMonth--;
            }
            this.writeLastSettlement(lastSettledYear, lastSettledMonth);
            writeln(`首次运行或无结算记录，已设置基准结算点为: ${lastSettledYear}年${pad2(lastSettledMonth)}月。`);
            return;
        }

        let yearToSettle = lastSettledYear;
        let monthToSettle = lastSettledMonth;

        while (true) {
            monthToSettle++;
            if (monthToSettle > 12) {
                monthToSettle = 1;
                yearToSettle++;
            }

            if (yearToSettle > currentYear || (yearToSettle === currentYear && monthToSettle >= currentMonth)) {
                break;
            }

            writeln(`\n>>> 开始自动结算: ${yearToSettle}年${pad2(monthToSettle)}月 <<`);
            this.generateMonthlyReportForSettlement(yearToSettle, monthToSettle);
            this.writeLastSettlement(yearToSettle, monthToSettle);
            writeln(`>>> 自动结算完成: ${yearToSettle}年${pad2(monthToSettle)}月 <<`);
        }
    }

    private async deleteExpense(): Promise<void> {
        if (this.expenseCount === 0) {
            writeln("没有开销记录可供删除。");
            return;
        }

        writeln("\n--- 删除开销记录 ---");
        writeln("以下是所有开销记录:");
        printExpenseHeaderWithIndex();
        for (let i = 0; i < this.expenseCount; i++) {
            printExpenseRowWithIndex(i + 1, this.allExpenses[i]!);
        }
        writeln(repeatChar("-", 77));

        write("请输入要删除的记录序号 (0 取消删除): ");
        let recordNumber = 0;
        while (true) {
            const { value, ok } = await this.reader.readInt();
            if (ok && value >= 0 && value <= this.expenseCount) {
                recordNumber = value;
                break;
            }
            write(`输入无效。请输入 1 到 ${this.expenseCount} 之间的数字，或 0 取消: `);
        }

        if (recordNumber === 0) {
            writeln("取消删除操作。");
            return;
        }

        const indexToDelete = recordNumber - 1;

        writeln("\n即将删除以下记录:");
        printExpenseHeader();
        printExpenseRow(this.allExpenses[indexToDelete]!);
        writeln(repeatChar("-", 72));

        // First confirmation
        write("确认删除吗？ (y/n): ");
        const confirm = await this.reader.readLine();

        if (confirm.length > 0 && (confirm[0] === "y" || confirm[0] === "Y")) {
            // Second confirmation
            writeln("\n警告：此操作无法撤销！");
            write("最后一次确认，真的要删除这条记录吗？ (y/n): ");
            const finalConfirm = await this.reader.readLine();

            if (finalConfirm.length > 0 && (finalConfirm[0] === "y" || finalConfirm[0] === "Y")) {
                writeln("\n正在删除记录...");
                for (let i = indexToDelete; i < this.expenseCount - 1; i++) {
                    const next = this.allExpenses[i + 1]!;
                    this.allExpenses[i]!.setData(
                        next.year, next.month, next.day,
                        next.description, next.amount, next.category
                    );
                }
                this.expenseCount--;
                writeln("记录已删除。");
                this.saveExpenses();
                writeln("数据已自动保存。");
            } else {
                writeln("已取消删除操作（二次确认未通过）。");
            }
        } else {
            writeln("取消删除操作。");
        }
    }
}

function splitN(s: string, sep: string, n: number): string[] {
    const result: string[] = [];
    let remaining = s;
    for (let i = 0; i < n - 1; i++) {
        const idx = remaining.indexOf(sep);
        if (idx < 0) break;
        result.push(remaining.substring(0, idx));
        remaining = remaining.substring(idx + 1);
    }
    if (remaining.length > 0 || result.length > 0) {
        result.push(remaining);
    }
    return result;
}

async function main(): Promise<void> {
    const reader = new LineReader();
    const tracker = new ExpenseTracker(reader);
    await tracker.init();
    await tracker.run();
    reader.close();
}

main();

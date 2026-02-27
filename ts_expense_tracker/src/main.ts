import { readFileSync, writeFileSync, readSync } from "node:fs";

const MAX_EXPENSES = 1000;
const MAX_UNIQUE_CATEGORIES_PER_MONTH = 20;
const DATA_FILE = "expenses.dat";
const SETTLEMENT_FILE = "settlement_status.txt";
const MAX_DESCRIPTION_LENGTH = 100;
const MAX_CATEGORY_LENGTH = 50;

// ── Pre-computed constants ──
const DASH_72 = "-".repeat(72);
const DASH_77 = "-".repeat(77);
const DASH_30 = "-".repeat(30);
const NL = "\n";

// ── Output buffer — collect all writes and flush at strategic points ──
let outBuf: string[] = [];

function flushOut(): void {
    if (outBuf.length > 0) {
        process.stdout.write(outBuf.join(""));
        outBuf.length = 0;
    }
}

function write(s: string): void {
    outBuf.push(s);
}

function writeln(s: string): void {
    outBuf.push(s);
    outBuf.push(NL);
}

// ── Synchronous stdin reader using fd 0 ──
const stdinBuf = Buffer.allocUnsafe(4096);
let stdinRemainder = "";

function readLineSync(): string {
    while (true) {
        const nlIdx = stdinRemainder.indexOf("\n");
        if (nlIdx !== -1) {
            const line = stdinRemainder.substring(0, nlIdx);
            stdinRemainder = stdinRemainder.substring(nlIdx + 1);
            return line.endsWith("\r") ? line.substring(0, line.length - 1) : line;
        }
        let bytesRead: number;
        try {
            bytesRead = readSync(0, stdinBuf, 0, 4096, null);
        } catch {
            const rest = stdinRemainder;
            stdinRemainder = "";
            return rest;
        }
        if (bytesRead === 0) {
            const result = stdinRemainder;
            stdinRemainder = "";
            return result;
        }
        stdinRemainder += stdinBuf.toString("utf8", 0, bytesRead);
    }
}

function readIntSync(): { value: number; ok: boolean } {
    const line = readLineSync();
    const trimmed = line.trim();
    if (trimmed === "") return { value: 0, ok: false };
    const n = parseInt(trimmed, 10);
    if (isNaN(n)) return { value: 0, ok: false };
    // Make sure the entire string is a valid integer
    const nStr = n.toString();
    if (nStr !== trimmed && ("+" + nStr) !== trimmed) {
        if (!/^[+-]?\d+$/.test(trimmed)) {
            return { value: 0, ok: false };
        }
    }
    return { value: n, ok: true };
}

// ── Expense storage using parallel arrays (SoA) for better cache locality ──
// We keep a class facade for API compatibility but store data in typed arrays where possible
const expYears = new Int32Array(MAX_EXPENSES);
const expMonths = new Int32Array(MAX_EXPENSES);
const expDays = new Int32Array(MAX_EXPENSES);
const expAmounts = new Float64Array(MAX_EXPENSES);
const expDescriptions: string[] = new Array(MAX_EXPENSES);
const expCategories: string[] = new Array(MAX_EXPENSES);
let expenseCount = 0;

function setExpense(idx: number, y: number, m: number, d: number, desc: string, amt: number, cat: string): void {
    expYears[idx] = y;
    expMonths[idx] = m;
    expDays[idx] = d;
    expAmounts[idx] = amt;
    expDescriptions[idx] = desc;
    expCategories[idx] = cat;
}

// ── Helpers — small monomorphic functions for V8 inlining ──

function pad2(n: number): string {
    return n < 10 ? "0" + n : "" + n;
}

function padRight(s: string, w: number): string {
    const len = s.length;
    if (len >= w) return s;
    return s + " ".repeat(w - len);
}

function padLeft(s: string, w: number): string {
    const len = s.length;
    if (len >= w) return s;
    return " ".repeat(w - len) + s;
}

function fmtAmount(n: number): string {
    return n.toFixed(2);
}

function formatDateStr(idx: number): string {
    return expYears[idx] + "-" + pad2(expMonths[idx]!) + "-" + pad2(expDays[idx]!);
}

function appendExpenseHeader(buf: string[]): void {
    buf.push(padRight("日期", 12));
    buf.push(padRight("描述", 30));
    buf.push(padRight("类别", 20));
    buf.push(padLeft("金额", 10));
    buf.push(NL);
    buf.push(DASH_72);
    buf.push(NL);
}

function appendExpenseHeaderWithIndex(buf: string[]): void {
    buf.push(padRight("序号", 5));
    buf.push(padRight("日期", 12));
    buf.push(padRight("描述", 30));
    buf.push(padRight("类别", 20));
    buf.push(padLeft("金额", 10));
    buf.push(NL);
    buf.push(DASH_77);
    buf.push(NL);
}

function appendExpenseRow(buf: string[], idx: number): void {
    buf.push(padRight(formatDateStr(idx), 12));
    buf.push(padRight(expDescriptions[idx]!, 30));
    buf.push(padRight(expCategories[idx]!, 20));
    buf.push(padLeft(fmtAmount(expAmounts[idx]!), 10));
    buf.push(NL);
}

function appendExpenseRowWithIndex(buf: string[], dispIdx: number, idx: number): void {
    buf.push(padRight("" + dispIdx, 5));
    buf.push(padRight(formatDateStr(idx), 12));
    buf.push(padRight(expDescriptions[idx]!, 30));
    buf.push(padRight(expCategories[idx]!, 20));
    buf.push(padLeft(fmtAmount(expAmounts[idx]!), 10));
    buf.push(NL);
}

// For interactive output that uses write/writeln already going to outBuf,
// we provide header/row functions that write to outBuf directly
function printExpenseHeader(): void {
    appendExpenseHeader(outBuf);
}

function printExpenseRow(idx: number): void {
    appendExpenseRow(outBuf, idx);
}

function printExpenseHeaderWithIndex(): void {
    appendExpenseHeaderWithIndex(outBuf);
}

function printExpenseRowWithIndex(dispIdx: number, idx: number): void {
    appendExpenseRowWithIndex(outBuf, dispIdx, idx);
}

// ── splitN ──
function splitN(s: string, sep: string, n: number): string[] {
    const result: string[] = [];
    let start = 0;
    for (let i = 0; i < n - 1; i++) {
        const idx = s.indexOf(sep, start);
        if (idx < 0) break;
        result.push(s.substring(start, idx));
        start = idx + 1;
    }
    if (start < s.length || result.length > 0) {
        result.push(s.substring(start));
    }
    return result;
}

// ── File I/O ──

function saveExpenses(): void {
    const parts: string[] = new Array(expenseCount + 1);
    parts[0] = "" + expenseCount;
    for (let i = 0; i < expenseCount; i++) {
        parts[i + 1] = expYears[i] + "," + expMonths[i] + "," + expDays[i] + "," +
            expDescriptions[i] + "," + expAmounts[i] + "," + expCategories[i];
    }
    try {
        writeFileSync(DATA_FILE, parts.join(NL) + NL, "utf-8");
    } catch {
        process.stderr.write("错误：无法打开文件 " + DATA_FILE + " 进行写入！\n");
    }
}

function loadExpenses(): boolean {
    let content: string;
    try {
        content = readFileSync(DATA_FILE, "utf-8");
    } catch {
        return false;
    }

    const lines = content.trimEnd().split("\n");
    if (lines.length === 0) {
        expenseCount = 0;
        return false;
    }

    const countFromFile = parseInt(lines[0]!.trim(), 10);
    if (isNaN(countFromFile) || countFromFile < 0 || countFromFile > MAX_EXPENSES) {
        expenseCount = 0;
        return false;
    }

    let loadedCount = 0;
    for (let i = 1; i < lines.length && i - 1 < countFromFile; i++) {
        if (loadedCount >= MAX_EXPENSES) break;

        const line = lines[i]!;
        const parts = splitN(line, ",", 6);

        // Parse year
        if (parts.length < 1) {
            process.stderr.write("警告：记录 " + i + " 数据不完整 (年份)。\n");
            continue;
        }
        const year = parseInt(parts[0]!, 10);
        if (isNaN(year)) {
            process.stderr.write("警告：无效年份格式 '" + parts[0] + "' 在记录 " + i + "。跳过此记录。\n");
            continue;
        }

        // Parse month
        if (parts.length < 2) {
            process.stderr.write("警告：记录 " + i + " 数据不完整 (月份)。\n");
            continue;
        }
        const month = parseInt(parts[1]!, 10);
        if (isNaN(month)) {
            process.stderr.write("警告：无效月份格式 '" + parts[1] + "' 在记录 " + i + "。跳过此记录。\n");
            continue;
        }

        // Parse day
        if (parts.length < 3) {
            process.stderr.write("警告：记录 " + i + " 数据不完整 (日期)。\n");
            continue;
        }
        const day = parseInt(parts[2]!, 10);
        if (isNaN(day)) {
            process.stderr.write("警告：无效日期格式 '" + parts[2] + "' 在记录 " + i + "。跳过此记录。\n");
            continue;
        }

        // Parse description
        if (parts.length < 4) {
            process.stderr.write("警告：记录 " + i + " 数据不完整 (描述)。\n");
            continue;
        }
        let descriptionStr = parts[3]!;
        if (descriptionStr.length > MAX_DESCRIPTION_LENGTH) {
            descriptionStr = descriptionStr.substring(0, MAX_DESCRIPTION_LENGTH);
        }

        // Parse amount
        if (parts.length < 5) {
            process.stderr.write("警告：记录 " + i + " 数据不完整 (金额)。\n");
            continue;
        }
        const amount = parseFloat(parts[4]!);
        if (isNaN(amount)) {
            process.stderr.write("警告：无效金额格式 '" + parts[4] + "' 在记录 " + i + "。跳过此记录。\n");
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

        setExpense(loadedCount, year, month, day, descriptionStr, amount, categoryStr);
        loadedCount++;
    }

    expenseCount = loadedCount;
    return true;
}

// ── Settlement ──

function readLastSettlement(): [number, number] {
    let lastYear = 0;
    let lastMonth = 0;
    try {
        const content = readFileSync(SETTLEMENT_FILE, "utf-8");
        const parts = content.trim().split(/\s+/);
        if (parts[0]) lastYear = parseInt(parts[0], 10) || 0;
        if (parts[1]) lastMonth = parseInt(parts[1], 10) || 0;
    } catch {
        // File doesn't exist
    }
    return [lastYear, lastMonth];
}

function writeLastSettlement(year: number, month: number): void {
    try {
        writeFileSync(SETTLEMENT_FILE, year + " " + month + "\n", "utf-8");
    } catch {
        process.stderr.write("错误：无法写入结算状态文件 " + SETTLEMENT_FILE + "\n");
    }
}

function generateMonthlyReportForSettlement(year: number, month: number): void {
    writeln("\n--- " + year + "年" + pad2(month) + "月 开销报告 (自动结算) ---");

    let totalMonthAmount = 0;
    let foundRecords = false;
    const catNames: string[] = [];
    const catTotals: number[] = [];
    let catCount = 0;

    writeln("明细:");
    printExpenseHeader();

    for (let i = 0; i < expenseCount; i++) {
        if (expYears[i] === year && expMonths[i] === month) {
            foundRecords = true;
            printExpenseRow(i);
            const amt = expAmounts[i]!;
            totalMonthAmount += amt;

            const cat = expCategories[i]!;
            let categoryExists = false;
            for (let j = 0; j < catCount; j++) {
                if (catNames[j] === cat) {
                    catTotals[j]! += amt;
                    catTotals[j] = catTotals[j]!;
                    categoryExists = true;
                    break;
                }
            }
            if (!categoryExists && catCount < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
                catNames[catCount] = cat;
                catTotals[catCount] = amt;
                catCount++;
            }
        }
    }

    if (!foundRecords) {
        writeln("该月份没有开销记录。");
        return;
    }

    writeln(DASH_72);
    writeln(padRight("本月总计:", 62) + padLeft(fmtAmount(totalMonthAmount), 10));
    writeln("");

    if (catCount > 0) {
        writeln("按类别汇总:");
        writeln(padRight("类别", 20) + padLeft("总金额", 10));
        writeln(DASH_30);
        for (let j = 0; j < catCount; j++) {
            writeln(padRight(catNames[j]!, 20) + padLeft(fmtAmount(catTotals[j]!), 10));
        }
        writeln(DASH_30);
    }

    writeln("--- 报告生成完毕 ---");
}

function performAutomaticSettlement(): void {
    let [lastSettledYear, lastSettledMonth] = readLastSettlement();

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
        writeLastSettlement(lastSettledYear, lastSettledMonth);
        writeln("首次运行或无结算记录，已设置基准结算点为: " + lastSettledYear + "年" + pad2(lastSettledMonth) + "月。");
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

        writeln("\n>>> 开始自动结算: " + yearToSettle + "年" + pad2(monthToSettle) + "月 <<");
        generateMonthlyReportForSettlement(yearToSettle, monthToSettle);
        writeLastSettlement(yearToSettle, monthToSettle);
        writeln(">>> 自动结算完成: " + yearToSettle + "年" + pad2(monthToSettle) + "月 <<");
    }
}

// ── Expense operations ──

function addExpense(): void {
    if (expenseCount >= MAX_EXPENSES) {
        writeln("错误：开销记录已满！无法添加更多记录。");
        flushOut();
        return;
    }

    writeln("\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---");

    const now = new Date();
    const currentYear = now.getFullYear();
    const currentMonth = now.getMonth() + 1;
    const currentDay = now.getDate();

    // Get year
    write("输入年份 (YYYY) [默认: " + currentYear + ", -1 取消]: ");
    flushOut();
    let lineInput = readLineSync();
    if (lineInput === "-1") { writeln("已取消添加开销。"); flushOut(); return; }
    let year = currentYear;
    if (lineInput.trim() !== "") {
        const n = parseInt(lineInput.trim(), 10);
        if (!isNaN(n) && /^-?\d+$/.test(lineInput.trim())) {
            year = n;
        } else {
            writeln("年份输入无效或包含非数字字符，将使用默认年份: " + currentYear + "。");
        }
    }

    // Get month
    write("输入月份 (MM) [默认: " + currentMonth + ", -1 取消]: ");
    flushOut();
    lineInput = readLineSync();
    if (lineInput === "-1") { writeln("已取消添加开销。"); flushOut(); return; }
    let month = currentMonth;
    if (lineInput.trim() !== "") {
        const n = parseInt(lineInput.trim(), 10);
        if (!isNaN(n) && n >= 1 && n <= 12 && /^-?\d+$/.test(lineInput.trim())) {
            month = n;
        } else {
            writeln("月份输入无效或范围不正确 (1-12)，将使用默认月份: " + currentMonth + "。");
        }
    }

    // Get day
    write("输入日期 (DD) [默认: " + currentDay + ", -1 取消]: ");
    flushOut();
    lineInput = readLineSync();
    if (lineInput === "-1") { writeln("已取消添加开销。"); flushOut(); return; }
    let day = currentDay;
    if (lineInput.trim() !== "") {
        const n = parseInt(lineInput.trim(), 10);
        if (!isNaN(n) && n >= 1 && n <= 31 && /^-?\d+$/.test(lineInput.trim())) {
            day = n;
        } else {
            writeln("日期输入无效或范围不正确 (1-31)，将使用默认日期: " + currentDay + "。");
        }
    }

    // Basic date validation
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        writeln("日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。");
        flushOut();
        return;
    }

    // Get description
    write("输入描述 (最多 " + MAX_DESCRIPTION_LENGTH + " 字符, 输入 '!cancel' 取消): ");
    flushOut();
    let description = readLineSync();
    if (description === "!cancel") { writeln("已取消添加开销。"); flushOut(); return; }
    if (description.length > MAX_DESCRIPTION_LENGTH) {
        writeln("描述过长，已截断为 " + MAX_DESCRIPTION_LENGTH + " 字符。");
        description = description.substring(0, MAX_DESCRIPTION_LENGTH);
    }

    // Get amount
    write("输入金额 (-1 取消): ");
    flushOut();
    let amount = 0;
    while (true) {
        lineInput = readLineSync();
        if (lineInput === "-1") { writeln("已取消添加开销。"); flushOut(); return; }
        const a = parseFloat(lineInput.trim());
        if (!isNaN(a) && a >= 0 && /^[+-]?(\d+\.?\d*|\.\d+)$/.test(lineInput.trim())) {
            amount = a;
            break;
        }
        write("金额无效或为负，请重新输入 (-1 取消): ");
        flushOut();
    }

    // Get category
    write("输入类别 (如 餐饮, 交通, 娱乐; 最多 " + MAX_CATEGORY_LENGTH + " 字符, 输入 '!cancel' 取消): ");
    flushOut();
    let category = readLineSync();
    if (category === "!cancel") { writeln("已取消添加开销。"); flushOut(); return; }
    if (category.length > MAX_CATEGORY_LENGTH) {
        writeln("类别名称过长，已截断为 " + MAX_CATEGORY_LENGTH + " 字符。");
        category = category.substring(0, MAX_CATEGORY_LENGTH);
    }
    if (category === "") {
        category = "未分类";
    }

    setExpense(expenseCount, year, month, day, description, amount, category);
    expenseCount++;
    writeln("开销已添加。");
    flushOut();
}

function displayAllExpenses(): void {
    if (expenseCount === 0) {
        writeln("没有开销记录。");
        return;
    }
    writeln("\n--- 所有开销记录 ---");
    printExpenseHeader();
    for (let i = 0; i < expenseCount; i++) {
        printExpenseRow(i);
    }
    writeln(DASH_72);
}

function displayMonthlySummary(): void {
    writeln("\n--- 月度开销统计 ---");

    // Get year
    write("输入要统计的年份 (YYYY) (-1 取消): ");
    flushOut();
    let year = 0;
    while (true) {
        const { value, ok } = readIntSync();
        if (ok) {
            if (value === -1) { writeln("已取消月度统计。"); flushOut(); return; }
            year = value;
            break;
        }
        write("年份输入无效，请重新输入 (-1 取消): ");
        flushOut();
    }

    // Get month
    write("输入要统计的月份 (MM) (-1 取消): ");
    flushOut();
    let month = 0;
    while (true) {
        const { value, ok } = readIntSync();
        if (ok) {
            if (value === -1) { writeln("已取消月度统计。"); flushOut(); return; }
            if (value >= 1 && value <= 12) {
                month = value;
                break;
            }
            write("月份输入无效 (1-12)，请重新输入 (-1 取消): ");
            flushOut();
        } else {
            write("月份输入无效 (1-12)，请重新输入 (-1 取消): ");
            flushOut();
        }
    }

    writeln("\n--- " + year + "年" + pad2(month) + "月 开销统计 ---");

    let totalMonthAmount = 0;
    let foundRecords = false;
    const catNames: string[] = [];
    const catTotals: number[] = [];
    let catCount = 0;

    printExpenseHeader();

    for (let i = 0; i < expenseCount; i++) {
        if (expYears[i] === year && expMonths[i] === month) {
            foundRecords = true;
            printExpenseRow(i);
            const amt = expAmounts[i]!;
            totalMonthAmount += amt;

            const cat = expCategories[i]!;
            let categoryExists = false;
            for (let j = 0; j < catCount; j++) {
                if (catNames[j] === cat) {
                    catTotals[j]! += amt;
                    catTotals[j] = catTotals[j]!;
                    categoryExists = true;
                    break;
                }
            }
            if (!categoryExists && catCount < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
                catNames[catCount] = cat;
                catTotals[catCount] = amt;
                catCount++;
            }
        }
    }

    if (!foundRecords) {
        writeln("该月份没有开销记录。");
    } else {
        writeln(DASH_72);
        writeln(padRight("本月总计:", 62) + padLeft(fmtAmount(totalMonthAmount), 10));
        writeln("");

        if (catCount > 0) {
            writeln("按类别汇总:");
            writeln(padRight("类别", 20) + padLeft("总金额", 10));
            writeln(DASH_30);
            for (let j = 0; j < catCount; j++) {
                writeln(padRight(catNames[j]!, 20) + padLeft(fmtAmount(catTotals[j]!), 10));
            }
            writeln(DASH_30);
        }
    }
}

function listExpensesByPeriod(): void {
    while (true) {
        writeln("\n--- 按期间列出开销 --- ");
        writeln("1. 按年份列出");
        writeln("2. 按月份列出");
        writeln("3. 按日期列出");
        writeln("0. 返回主菜单");
        writeln("--------------------");
        write("请输入选项: ");
        flushOut();

        const { value: choice } = readIntSync();

        switch (choice) {
            case 1: {
                writeln("\n--- 按年份列出开销 ---");
                write("输入年份 (YYYY) (输入 0 返回): ");
                flushOut();
                let year = 0;
                while (true) {
                    const { value, ok } = readIntSync();
                    if (ok) { year = value; break; }
                    write("年份输入无效，请重新输入 (输入 0 返回): ");
                    flushOut();
                }
                if (year === 0) continue;

                let found = false;
                printExpenseHeader();
                for (let i = 0; i < expenseCount; i++) {
                    if (expYears[i] === year) {
                        printExpenseRow(i);
                        found = true;
                    }
                }
                if (!found) writeln("在 " + year + " 年没有找到开销记录。");
                writeln(DASH_72);
                flushOut();
                break;
            }
            case 2: {
                writeln("\n--- 按月份列出开销 ---");
                write("输入年份 (YYYY) (输入 0 返回): ");
                flushOut();
                let year = 0;
                while (true) {
                    const { value, ok } = readIntSync();
                    if (ok) { year = value; break; }
                    write("年份输入无效，请重新输入 (输入 0 返回): ");
                    flushOut();
                }
                if (year === 0) continue;

                write("输入月份 (MM) (输入 0 返回): ");
                flushOut();
                let month = 0;
                while (true) {
                    const { value, ok } = readIntSync();
                    if (ok && (value === 0 || (value >= 1 && value <= 12))) {
                        month = value; break;
                    }
                    write("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                    flushOut();
                }
                if (month === 0) continue;

                let found = false;
                printExpenseHeader();
                for (let i = 0; i < expenseCount; i++) {
                    if (expYears[i] === year && expMonths[i] === month) {
                        printExpenseRow(i);
                        found = true;
                    }
                }
                if (!found) writeln("在 " + year + " 年 " + month + " 月没有找到开销记录。");
                writeln(DASH_72);
                flushOut();
                break;
            }
            case 3: {
                writeln("\n--- 按日期列出开销 ---");
                write("输入年份 (YYYY) (输入 0 返回): ");
                flushOut();
                let year = 0;
                while (true) {
                    const { value, ok } = readIntSync();
                    if (ok) { year = value; break; }
                    write("年份输入无效，请重新输入 (输入 0 返回): ");
                    flushOut();
                }
                if (year === 0) continue;

                write("输入月份 (MM) (输入 0 返回): ");
                flushOut();
                let month = 0;
                while (true) {
                    const { value, ok } = readIntSync();
                    if (ok && (value === 0 || (value >= 1 && value <= 12))) {
                        month = value; break;
                    }
                    write("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                    flushOut();
                }
                if (month === 0) continue;

                write("输入日期 (DD) (输入 0 返回): ");
                flushOut();
                let day = 0;
                while (true) {
                    const { value, ok } = readIntSync();
                    if (ok && (value === 0 || (value >= 1 && value <= 31))) {
                        day = value; break;
                    }
                    write("日期输入无效 (1-31)，请重新输入 (输入 0 返回): ");
                    flushOut();
                }
                if (day === 0) continue;

                let found = false;
                printExpenseHeader();
                for (let i = 0; i < expenseCount; i++) {
                    if (expYears[i] === year && expMonths[i] === month && expDays[i] === day) {
                        printExpenseRow(i);
                        found = true;
                    }
                }
                if (!found) writeln("在 " + year + " 年 " + month + " 月 " + day + " 日没有找到开销记录。");
                writeln(DASH_72);
                flushOut();
                break;
            }
            case 0:
                writeln("返回主菜单...");
                flushOut();
                return;
            default:
                writeln("无效选项，请重试。");
                flushOut();
                break;
        }
    }
}

function deleteExpense(): void {
    if (expenseCount === 0) {
        writeln("没有开销记录可供删除。");
        flushOut();
        return;
    }

    writeln("\n--- 删除开销记录 ---");
    writeln("以下是所有开销记录:");
    printExpenseHeaderWithIndex();
    for (let i = 0; i < expenseCount; i++) {
        printExpenseRowWithIndex(i + 1, i);
    }
    writeln(DASH_77);

    write("请输入要删除的记录序号 (0 取消删除): ");
    flushOut();
    let recordNumber = 0;
    while (true) {
        const { value, ok } = readIntSync();
        if (ok && value >= 0 && value <= expenseCount) {
            recordNumber = value;
            break;
        }
        write("输入无效。请输入 1 到 " + expenseCount + " 之间的数字，或 0 取消: ");
        flushOut();
    }

    if (recordNumber === 0) {
        writeln("取消删除操作。");
        flushOut();
        return;
    }

    const indexToDelete = recordNumber - 1;

    writeln("\n即将删除以下记录:");
    printExpenseHeader();
    printExpenseRow(indexToDelete);
    writeln(DASH_72);

    // First confirmation
    write("确认删除吗？ (y/n): ");
    flushOut();
    const confirm = readLineSync();

    if (confirm.length > 0 && (confirm[0] === "y" || confirm[0] === "Y")) {
        // Second confirmation
        writeln("\n警告：此操作无法撤销！");
        write("最后一次确认，真的要删除这条记录吗？ (y/n): ");
        flushOut();
        const finalConfirm = readLineSync();

        if (finalConfirm.length > 0 && (finalConfirm[0] === "y" || finalConfirm[0] === "Y")) {
            writeln("\n正在删除记录...");
            for (let i = indexToDelete; i < expenseCount - 1; i++) {
                expYears[i] = expYears[i + 1]!;
                expMonths[i] = expMonths[i + 1]!;
                expDays[i] = expDays[i + 1]!;
                expAmounts[i] = expAmounts[i + 1]!;
                expDescriptions[i] = expDescriptions[i + 1]!;
                expCategories[i] = expCategories[i + 1]!;
            }
            expenseCount--;
            writeln("记录已删除。");
            saveExpenses();
            writeln("数据已自动保存。");
            flushOut();
        } else {
            writeln("已取消删除操作（二次确认未通过）。");
            flushOut();
        }
    } else {
        writeln("取消删除操作。");
        flushOut();
    }
}

// ── Main ──

function main(): void {
    // Init
    if (loadExpenses()) {
        writeln("成功加载 " + expenseCount + " 条历史记录。");
    } else {
        writeln("未找到历史数据文件或加载失败，开始新的记录。");
    }
    performAutomaticSettlement();
    flushOut();

    // Main loop
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
        flushOut();

        const { value: choice } = readIntSync();

        switch (choice) {
            case 1:
                addExpense();
                break;
            case 2:
                displayAllExpenses();
                flushOut();
                break;
            case 3:
                displayMonthlySummary();
                flushOut();
                break;
            case 4:
                listExpensesByPeriod();
                break;
            case 5:
                deleteExpense();
                break;
            case 6:
                saveExpenses();
                writeln("数据已保存。正在退出...");
                flushOut();
                return;
            default:
                writeln("无效选项，请重试。");
                flushOut();
                break;
        }
    }
}

main();

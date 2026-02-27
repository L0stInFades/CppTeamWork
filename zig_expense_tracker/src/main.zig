const std = @import("std");
const Io = std.Io;
const c = @cImport({
    @cInclude("time.h");
});

// ─── 全局常量 ───
const MAX_EXPENSES: usize = 1000;
const MAX_UNIQUE_CATEGORIES_PER_MONTH: usize = 20;
const DATA_FILE = "expenses.dat";
const SETTLEMENT_FILE = "settlement_status.txt";
const MAX_DESCRIPTION_LENGTH: usize = 100;
const MAX_CATEGORY_LENGTH: usize = 50;
const LINE_BUF_SIZE: usize = 1024;

// ─── Expense 结构体 ───
const Expense = struct {
    year: i32 = 0,
    month: i32 = 0,
    day: i32 = 0,
    desc_buf: [MAX_DESCRIPTION_LENGTH]u8 = [_]u8{0} ** MAX_DESCRIPTION_LENGTH,
    desc_len: usize = 0,
    amount: f64 = 0.0,
    cat_buf: [MAX_CATEGORY_LENGTH]u8 = [_]u8{0} ** MAX_CATEGORY_LENGTH,
    cat_len: usize = 0,

    fn setData(self: *Expense, y: i32, m: i32, d: i32, desc: []const u8, amt: f64, cat: []const u8) void {
        self.year = y;
        self.month = m;
        self.day = d;
        const dl = @min(desc.len, MAX_DESCRIPTION_LENGTH);
        @memcpy(self.desc_buf[0..dl], desc[0..dl]);
        self.desc_len = dl;
        self.amount = amt;
        const cl = @min(cat.len, MAX_CATEGORY_LENGTH);
        @memcpy(self.cat_buf[0..cl], cat[0..cl]);
        self.cat_len = cl;
    }

    fn getDescription(self: *const Expense) []const u8 {
        return self.desc_buf[0..self.desc_len];
    }

    fn getCategory(self: *const Expense) []const u8 {
        return self.cat_buf[0..self.cat_len];
    }
};

// ─── CategorySum 结构体 ───
const CategorySum = struct {
    name_buf: [MAX_CATEGORY_LENGTH]u8 = [_]u8{0} ** MAX_CATEGORY_LENGTH,
    name_len: usize = 0,
    total: f64 = 0.0,

    fn getName(self: *const CategorySum) []const u8 {
        return self.name_buf[0..self.name_len];
    }

    fn setName(self: *CategorySum, name: []const u8) void {
        const l = @min(name.len, MAX_CATEGORY_LENGTH);
        @memcpy(self.name_buf[0..l], name[0..l]);
        self.name_len = l;
    }
};

// ─── 辅助函数 ───

fn readLine(reader: *Io.Reader) !?[]const u8 {
    const line = reader.takeDelimiter('\n') catch |err| {
        if (err == error.StreamTooLong) {
            return null;
        }
        return err;
    };
    if (line) |l| {
        // 去除 Windows 换行符
        return std.mem.trimEnd(u8, l, "\r");
    }
    return null;
}

fn parseInt(s: []const u8) ?i32 {
    const trimmed = std.mem.trim(u8, s, " \t\r\n");
    if (trimmed.len == 0) return null;
    return std.fmt.parseInt(i32, trimmed, 10) catch null;
}

fn parseFloat(s: []const u8) ?f64 {
    const trimmed = std.mem.trim(u8, s, " \t\r\n");
    if (trimmed.len == 0) return null;
    return std.fmt.parseFloat(f64, trimmed) catch null;
}

fn writeRepeat(writer: *Io.Writer, byte: u8, count: usize) !void {
    for (0..count) |_| {
        try writer.writeByte(byte);
    }
}

fn padRight(writer: *Io.Writer, text: []const u8, width: usize) !void {
    try writer.writeAll(text);
    if (text.len < width) {
        try writeRepeat(writer, ' ', width - text.len);
    }
}

fn padLeft(writer: *Io.Writer, text: []const u8, width: usize) !void {
    if (text.len < width) {
        try writeRepeat(writer, ' ', width - text.len);
    }
    try writer.writeAll(text);
}

fn formatAmount(buf: []u8, amount: f64) []const u8 {
    var w: Io.Writer = .fixed(buf);
    w.print("{d:.2}", .{amount}) catch return "0.00";
    return w.buffered();
}

fn getCurrentDate() struct { year: i32, month: i32, day: i32 } {
    var now = c.time(null);
    const ltm = c.localtime(&now);
    return .{
        .year = @as(i32, @intCast(ltm.*.tm_year)) + 1900,
        .month = @as(i32, @intCast(ltm.*.tm_mon)) + 1,
        .day = @as(i32, @intCast(ltm.*.tm_mday)),
    };
}

fn printTableHeader(writer: *Io.Writer) !void {
    try padRight(writer, "日期", 12);
    try padRight(writer, "描述", 30);
    try padRight(writer, "类别", 20);
    try padLeft(writer, "金额", 10);
    try writer.writeByte('\n');
    try writeRepeat(writer, '-', 72);
    try writer.writeByte('\n');
}

fn printExpenseRow(writer: *Io.Writer, exp: *const Expense) !void {
    // 日期: YYYY-MM-DD
    var date_buf: [12]u8 = undefined;
    var date_w: Io.Writer = .fixed(&date_buf);
    date_w.print("{d}-{d:0>2}-{d:0>2}", .{
        exp.year,
        @as(u32, @intCast(if (exp.month >= 0) @as(u32, @intCast(exp.month)) else 0)),
        @as(u32, @intCast(if (exp.day >= 0) @as(u32, @intCast(exp.day)) else 0)),
    }) catch {};
    const date_str = date_w.buffered();
    try padRight(writer, date_str, 12);
    try padRight(writer, exp.getDescription(), 30);
    try padRight(writer, exp.getCategory(), 20);
    var amt_buf: [32]u8 = undefined;
    const amt_str = formatAmount(&amt_buf, exp.amount);
    try padLeft(writer, amt_str, 10);
    try writer.writeByte('\n');
}

// ─── ExpenseTracker 结构体 ───
const ExpenseTracker = struct {
    all_expenses: [MAX_EXPENSES]Expense = [_]Expense{.{}} ** MAX_EXPENSES,
    expense_count: usize = 0,
    io: Io,
    stdout_buf: [4096]u8 = undefined,
    stdin_buf: [LINE_BUF_SIZE]u8 = undefined,
    stdout_file_writer: Io.File.Writer = undefined,
    stdin_file_reader: Io.File.Reader = undefined,
    initialized: bool = false,

    fn create(io: Io) ExpenseTracker {
        return .{ .io = io };
    }

    fn ensureIo(self: *ExpenseTracker) void {
        if (!self.initialized) {
            self.stdout_file_writer = Io.File.Writer.initStreaming(.stdout(), self.io, &self.stdout_buf);
            self.stdin_file_reader = Io.File.Reader.init(.stdin(), self.io, &self.stdin_buf);
            self.initialized = true;
        }
    }

    fn writer(self: *ExpenseTracker) *Io.Writer {
        self.ensureIo();
        return &self.stdout_file_writer.interface;
    }

    fn reader(self: *ExpenseTracker) *Io.Reader {
        self.ensureIo();
        return &self.stdin_file_reader.interface;
    }

    fn flushStdout(self: *ExpenseTracker) !void {
        try self.writer().flush();
    }

    fn init(self: *ExpenseTracker) !void {
        const w = self.writer();
        if (self.loadExpenses()) {
            try w.print("成功加载 {d} 条历史记录。\n", .{self.expense_count});
        } else {
            try w.print("未找到历史数据文件或加载失败，开始新的记录。\n", .{});
        }
        try self.performAutomaticSettlement();
    }

    // ─── 主循环 ───
    fn run(self: *ExpenseTracker) !void {
        const w = self.writer();
        var choice: i32 = 0;
        while (true) {
            try w.print("\n大学生开销追踪器\n", .{});
            try w.print("--------------------\n", .{});
            try w.print("1. 添加开销记录\n", .{});
            try w.print("2. 查看所有开销\n", .{});
            try w.print("3. 查看月度统计\n", .{});
            try w.print("4. 按期间列出开销\n", .{});
            try w.print("5. 删除开销记录\n", .{});
            try w.print("6. 保存并退出\n", .{});
            try w.print("--------------------\n", .{});
            try w.print("请输入选项: ", .{});
            try self.flushStdout();

            const line = try readLine(self.reader()) orelse {
                try w.print("无效选项，请重试。\n", .{});
                continue;
            };
            choice = parseInt(line) orelse {
                try w.print("无效选项，请重试。\n", .{});
                continue;
            };

            switch (choice) {
                1 => try self.addExpense(),
                2 => try self.displayAllExpenses(),
                3 => try self.displayMonthlySummary(),
                4 => try self.listExpensesByPeriod(),
                5 => try self.deleteExpense(),
                6 => {
                    try self.saveExpenses();
                    try w.print("数据已保存。正在退出...\n", .{});
                    try self.flushStdout();
                    break;
                },
                else => try w.print("无效选项，请重试。\n", .{}),
            }
        }
    }

    // ─── 添加开销 ───
    fn addExpense(self: *ExpenseTracker) !void {
        const w = self.writer();
        if (self.expense_count >= MAX_EXPENSES) {
            try w.print("错误：开销记录已满！无法添加更多记录。\n", .{});
            return;
        }

        const today = getCurrentDate();
        var year: i32 = today.year;
        var month: i32 = today.month;
        var day: i32 = today.day;

        try w.print("\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---\n", .{});

        // 获取年份
        try w.print("输入年份 (YYYY) [默认: {d}, -1 取消]: ", .{today.year});
        try self.flushStdout();
        if (try readLine(self.reader())) |line| {
            const trimmed = std.mem.trim(u8, line, " \t\r\n");
            if (std.mem.eql(u8, trimmed, "-1")) {
                try w.print("已取消添加开销。\n", .{});
                return;
            }
            if (trimmed.len > 0) {
                if (parseInt(trimmed)) |v| {
                    year = v;
                } else {
                    try w.print("年份输入无效或包含非数字字符，将使用默认年份: {d}。\n", .{today.year});
                    year = today.year;
                }
            }
        }

        // 获取月份
        try w.print("输入月份 (MM) [默认: {d}, -1 取消]: ", .{today.month});
        try self.flushStdout();
        if (try readLine(self.reader())) |line| {
            const trimmed = std.mem.trim(u8, line, " \t\r\n");
            if (std.mem.eql(u8, trimmed, "-1")) {
                try w.print("已取消添加开销。\n", .{});
                return;
            }
            if (trimmed.len > 0) {
                if (parseInt(trimmed)) |v| {
                    if (v < 1 or v > 12) {
                        try w.print("月份输入无效或范围不正确 (1-12)，将使用默认月份: {d}。\n", .{today.month});
                        month = today.month;
                    } else {
                        month = v;
                    }
                } else {
                    try w.print("月份输入无效或范围不正确 (1-12)，将使用默认月份: {d}。\n", .{today.month});
                    month = today.month;
                }
            }
        }

        // 获取日期
        try w.print("输入日期 (DD) [默认: {d}, -1 取消]: ", .{today.day});
        try self.flushStdout();
        if (try readLine(self.reader())) |line| {
            const trimmed = std.mem.trim(u8, line, " \t\r\n");
            if (std.mem.eql(u8, trimmed, "-1")) {
                try w.print("已取消添加开销。\n", .{});
                return;
            }
            if (trimmed.len > 0) {
                if (parseInt(trimmed)) |v| {
                    if (v < 1 or v > 31) {
                        try w.print("日期输入无效或范围不正确 (1-31)，将使用默认日期: {d}。\n", .{today.day});
                        day = today.day;
                    } else {
                        day = v;
                    }
                } else {
                    try w.print("日期输入无效或范围不正确 (1-31)，将使用默认日期: {d}。\n", .{today.day});
                    day = today.day;
                }
            }
        }

        // 日期二次校验
        if (month < 1 or month > 12 or day < 1 or day > 31) {
            try w.print("日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。\n", .{});
            return;
        }

        // 获取描述
        try w.print("输入描述 (最多 {d} 字符, 输入 '!cancel' 取消): ", .{MAX_DESCRIPTION_LENGTH});
        try self.flushStdout();
        var desc_storage: [MAX_DESCRIPTION_LENGTH]u8 = undefined;
        var desc_len: usize = 0;
        if (try readLine(self.reader())) |line| {
            if (std.mem.eql(u8, line, "!cancel")) {
                try w.print("已取消添加开销。\n", .{});
                return;
            }
            if (line.len > MAX_DESCRIPTION_LENGTH) {
                try w.print("描述过长，已截断为 {d} 字符。\n", .{MAX_DESCRIPTION_LENGTH});
                desc_len = MAX_DESCRIPTION_LENGTH;
            } else {
                desc_len = line.len;
            }
            @memcpy(desc_storage[0..desc_len], line[0..desc_len]);
        }

        // 获取金额
        try w.print("输入金额 (-1 取消): ", .{});
        try self.flushStdout();
        var amount: f64 = 0.0;
        while (true) {
            if (try readLine(self.reader())) |line| {
                const trimmed = std.mem.trim(u8, line, " \t\r\n");
                if (std.mem.eql(u8, trimmed, "-1")) {
                    try w.print("已取消添加开销。\n", .{});
                    return;
                }
                if (parseFloat(trimmed)) |v| {
                    if (v >= 0) {
                        amount = v;
                        break;
                    }
                }
            }
            try w.print("金额无效或为负，请重新输入 (-1 取消): ", .{});
            try self.flushStdout();
        }

        // 获取类别
        try w.print("输入类别 (如 餐饮, 交通, 娱乐; 最多 {d} 字符, 输入 '!cancel' 取消): ", .{MAX_CATEGORY_LENGTH});
        try self.flushStdout();
        var cat_storage: [MAX_CATEGORY_LENGTH]u8 = undefined;
        var cat_len: usize = 0;
        if (try readLine(self.reader())) |line| {
            if (std.mem.eql(u8, line, "!cancel")) {
                try w.print("已取消添加开销。\n", .{});
                return;
            }
            if (line.len > MAX_CATEGORY_LENGTH) {
                try w.print("类别名称过长，已截断为 {d} 字符。\n", .{MAX_CATEGORY_LENGTH});
                cat_len = MAX_CATEGORY_LENGTH;
            } else {
                cat_len = line.len;
            }
            @memcpy(cat_storage[0..cat_len], line[0..cat_len]);
        }

        // 空类别默认为"未分类"
        const category = if (cat_len == 0) "未分类" else cat_storage[0..cat_len];

        self.all_expenses[self.expense_count].setData(
            year,
            month,
            day,
            desc_storage[0..desc_len],
            amount,
            category,
        );
        self.expense_count += 1;
        try w.print("开销已添加。\n", .{});
    }

    // ─── 显示所有开销 ───
    fn displayAllExpenses(self: *ExpenseTracker) !void {
        const w = self.writer();
        if (self.expense_count == 0) {
            try w.print("没有开销记录。\n", .{});
            return;
        }
        try w.print("\n--- 所有开销记录 ---\n", .{});
        try printTableHeader(w);
        for (0..self.expense_count) |i| {
            try printExpenseRow(w, &self.all_expenses[i]);
        }
        try writeRepeat(w, '-', 72);
        try w.writeByte('\n');
        try self.flushStdout();
    }

    // ─── 月度统计 ───
    fn displayMonthlySummary(self: *ExpenseTracker) !void {
        const w = self.writer();
        try w.print("\n--- 月度开销统计 ---\n", .{});

        // 获取年份
        try w.print("输入要统计的年份 (YYYY) (-1 取消): ", .{});
        try self.flushStdout();
        var year: i32 = 0;
        while (true) {
            if (try readLine(self.reader())) |line| {
                if (parseInt(line)) |v| {
                    if (v == -1) {
                        try w.print("已取消月度统计。\n", .{});
                        return;
                    }
                    year = v;
                    break;
                }
            }
            try w.print("年份输入无效，请重新输入 (-1 取消): ", .{});
            try self.flushStdout();
        }

        // 获取月份
        try w.print("输入要统计的月份 (MM) (-1 取消): ", .{});
        try self.flushStdout();
        var month: i32 = 0;
        while (true) {
            if (try readLine(self.reader())) |line| {
                if (parseInt(line)) |v| {
                    if (v == -1) {
                        try w.print("已取消月度统计。\n", .{});
                        return;
                    }
                    if (v >= 1 and v <= 12) {
                        month = v;
                        break;
                    }
                }
            }
            try w.print("月份输入无效 (1-12)，请重新输入 (-1 取消): ", .{});
            try self.flushStdout();
        }

        try self.printMonthlyReport(year, month, false);
    }

    // ─── 打印月度报告（复用于月度统计和自动结算）───
    fn printMonthlyReport(self: *ExpenseTracker, year: i32, month: i32, is_settlement: bool) !void {
        const w = self.writer();
        const m_u32: u32 = @intCast(if (month >= 0) @as(u32, @intCast(month)) else 0);
        if (is_settlement) {
            try w.print("\n--- {d}年{d:0>2}月 开销报告 (自动结算) ---\n", .{ year, m_u32 });
            try w.print("明细:\n", .{});
        } else {
            try w.print("\n--- {d}年{d:0>2}月 开销统计 ---\n", .{ year, m_u32 });
        }

        var total_month_amount: f64 = 0;
        var found_records = false;
        var category_sums: [MAX_UNIQUE_CATEGORIES_PER_MONTH]CategorySum = [_]CategorySum{.{}} ** MAX_UNIQUE_CATEGORIES_PER_MONTH;
        var unique_categories_count: usize = 0;

        try printTableHeader(w);

        for (0..self.expense_count) |i| {
            const exp = &self.all_expenses[i];
            if (exp.year == year and exp.month == month) {
                found_records = true;
                try printExpenseRow(w, exp);
                total_month_amount += exp.amount;

                // 按类别汇总
                const cat = exp.getCategory();
                var category_exists = false;
                for (0..unique_categories_count) |j| {
                    if (std.mem.eql(u8, category_sums[j].getName(), cat)) {
                        category_sums[j].total += exp.amount;
                        category_exists = true;
                        break;
                    }
                }
                if (!category_exists and unique_categories_count < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
                    category_sums[unique_categories_count].setName(cat);
                    category_sums[unique_categories_count].total = exp.amount;
                    unique_categories_count += 1;
                }
            }
        }

        if (!found_records) {
            try w.print("该月份没有开销记录。\n", .{});
            if (is_settlement) return;
        } else {
            try writeRepeat(w, '-', 72);
            try w.writeByte('\n');

            // 本月总计
            try padRight(w, "本月总计:", 62);
            var amt_buf: [32]u8 = undefined;
            const amt_str = formatAmount(&amt_buf, total_month_amount);
            try padLeft(w, amt_str, 10);
            try w.print("\n\n", .{});

            // 按类别汇总
            if (unique_categories_count > 0) {
                try w.print("按类别汇总:\n", .{});
                try padRight(w, "类别", 20);
                try padLeft(w, "总金额", 10);
                try w.writeByte('\n');
                try writeRepeat(w, '-', 30);
                try w.writeByte('\n');
                for (0..unique_categories_count) |i| {
                    try padRight(w, category_sums[i].getName(), 20);
                    var cat_amt_buf: [32]u8 = undefined;
                    const cat_amt_str = formatAmount(&cat_amt_buf, category_sums[i].total);
                    try padLeft(w, cat_amt_str, 10);
                    try w.writeByte('\n');
                }
                try writeRepeat(w, '-', 30);
                try w.writeByte('\n');
            }
        }

        if (is_settlement and found_records) {
            try w.print("--- 报告生成完毕 ---\n", .{});
        }
        try self.flushStdout();
    }

    // ─── 按期间列出开销 ───
    fn listExpensesByPeriod(self: *ExpenseTracker) !void {
        const w = self.writer();
        var choice: i32 = 0;

        while (true) {
            try w.print("\n--- 按期间列出开销 --- \n", .{});
            try w.print("1. 按年份列出\n", .{});
            try w.print("2. 按月份列出\n", .{});
            try w.print("3. 按日期列出\n", .{});
            try w.print("0. 返回主菜单\n", .{});
            try w.print("--------------------\n", .{});
            try w.print("请输入选项: ", .{});
            try self.flushStdout();

            if (try readLine(self.reader())) |line| {
                choice = parseInt(line) orelse {
                    try w.print("无效选项，请重试。\n", .{});
                    continue;
                };
            } else {
                continue;
            }

            switch (choice) {
                1 => {
                    // 按年份列出
                    try w.print("\n--- 按年份列出开销 ---\n", .{});
                    try w.print("输入年份 (YYYY) (输入 0 返回): ", .{});
                    try self.flushStdout();
                    var year: i32 = 0;
                    while (true) {
                        if (try readLine(self.reader())) |line| {
                            if (parseInt(line)) |v| {
                                year = v;
                                break;
                            }
                        }
                        try w.print("年份输入无效，请重新输入 (输入 0 返回): ", .{});
                        try self.flushStdout();
                    }
                    if (year == 0) continue;

                    var found = false;
                    try printTableHeader(w);
                    for (0..self.expense_count) |i| {
                        if (self.all_expenses[i].year == year) {
                            try printExpenseRow(w, &self.all_expenses[i]);
                            found = true;
                        }
                    }
                    if (!found) {
                        try w.print("在 {d} 年没有找到开销记录。\n", .{year});
                    }
                    try writeRepeat(w, '-', 72);
                    try w.writeByte('\n');
                    try self.flushStdout();
                },
                2 => {
                    // 按月份列出
                    try w.print("\n--- 按月份列出开销 ---\n", .{});
                    try w.print("输入年份 (YYYY) (输入 0 返回): ", .{});
                    try self.flushStdout();
                    var year: i32 = 0;
                    while (true) {
                        if (try readLine(self.reader())) |line| {
                            if (parseInt(line)) |v| {
                                year = v;
                                break;
                            }
                        }
                        try w.print("年份输入无效，请重新输入 (输入 0 返回): ", .{});
                        try self.flushStdout();
                    }
                    if (year == 0) continue;

                    try w.print("输入月份 (MM) (输入 0 返回): ", .{});
                    try self.flushStdout();
                    var month: i32 = 0;
                    while (true) {
                        if (try readLine(self.reader())) |line| {
                            if (parseInt(line)) |v| {
                                if (v == 0 or (v >= 1 and v <= 12)) {
                                    month = v;
                                    break;
                                }
                            }
                        }
                        try w.print("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ", .{});
                        try self.flushStdout();
                    }
                    if (month == 0) continue;

                    var found = false;
                    try printTableHeader(w);
                    for (0..self.expense_count) |i| {
                        if (self.all_expenses[i].year == year and self.all_expenses[i].month == month) {
                            try printExpenseRow(w, &self.all_expenses[i]);
                            found = true;
                        }
                    }
                    if (!found) {
                        try w.print("在 {d} 年 {d} 月没有找到开销记录。\n", .{ year, month });
                    }
                    try writeRepeat(w, '-', 72);
                    try w.writeByte('\n');
                    try self.flushStdout();
                },
                3 => {
                    // 按日期列出
                    try w.print("\n--- 按日期列出开销 ---\n", .{});
                    try w.print("输入年份 (YYYY) (输入 0 返回): ", .{});
                    try self.flushStdout();
                    var year: i32 = 0;
                    while (true) {
                        if (try readLine(self.reader())) |line| {
                            if (parseInt(line)) |v| {
                                year = v;
                                break;
                            }
                        }
                        try w.print("年份输入无效，请重新输入 (输入 0 返回): ", .{});
                        try self.flushStdout();
                    }
                    if (year == 0) continue;

                    try w.print("输入月份 (MM) (输入 0 返回): ", .{});
                    try self.flushStdout();
                    var month: i32 = 0;
                    while (true) {
                        if (try readLine(self.reader())) |line| {
                            if (parseInt(line)) |v| {
                                if (v == 0 or (v >= 1 and v <= 12)) {
                                    month = v;
                                    break;
                                }
                            }
                        }
                        try w.print("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ", .{});
                        try self.flushStdout();
                    }
                    if (month == 0) continue;

                    try w.print("输入日期 (DD) (输入 0 返回): ", .{});
                    try self.flushStdout();
                    var day: i32 = 0;
                    while (true) {
                        if (try readLine(self.reader())) |line| {
                            if (parseInt(line)) |v| {
                                if (v == 0 or (v >= 1 and v <= 31)) {
                                    day = v;
                                    break;
                                }
                            }
                        }
                        try w.print("日期输入无效 (1-31)，请重新输入 (输入 0 返回): ", .{});
                        try self.flushStdout();
                    }
                    if (day == 0) continue;

                    var found = false;
                    try printTableHeader(w);
                    for (0..self.expense_count) |i| {
                        if (self.all_expenses[i].year == year and self.all_expenses[i].month == month and self.all_expenses[i].day == day) {
                            try printExpenseRow(w, &self.all_expenses[i]);
                            found = true;
                        }
                    }
                    if (!found) {
                        try w.print("在 {d} 年 {d} 月 {d} 日没有找到开销记录。\n", .{ year, month, day });
                    }
                    try writeRepeat(w, '-', 72);
                    try w.writeByte('\n');
                    try self.flushStdout();
                },
                0 => {
                    try w.print("返回主菜单...\n", .{});
                    break;
                },
                else => {
                    try w.print("无效选项，请重试。\n", .{});
                },
            }
        }
    }

    // ─── 保存开销数据 ───
    fn saveExpenses(self: *const ExpenseTracker) !void {
        const io = self.io;
        const file = Io.Dir.createFile(.cwd(), io, DATA_FILE, .{}) catch {
            var stderr_buf: [256]u8 = undefined;
            var stderr_fw = Io.File.Writer.initStreaming(.stderr(), io, &stderr_buf);
            stderr_fw.interface.print("错误：无法打开文件 {s} 进行写入！\n", .{DATA_FILE}) catch {};
            stderr_fw.interface.flush() catch {};
            return;
        };
        defer file.close(io);
        var file_buf: [4096]u8 = undefined;
        var fw = Io.File.Writer.initStreaming(file, io, &file_buf);
        const fwriter = &fw.interface;

        try fwriter.print("{d}\n", .{self.expense_count});
        for (0..self.expense_count) |i| {
            const exp = &self.all_expenses[i];
            try fwriter.print("{d},{d},{d},{s},{d:.2},{s}\n", .{
                exp.year,
                exp.month,
                exp.day,
                exp.getDescription(),
                exp.amount,
                exp.getCategory(),
            });
        }
        try fwriter.flush();
    }

    // ─── 加载开销数据 ───
    fn loadExpenses(self: *ExpenseTracker) bool {
        const io = self.io;
        const file = Io.Dir.openFile(.cwd(), io, DATA_FILE, .{}) catch {
            return false;
        };
        defer file.close(io);
        var file_buf: [LINE_BUF_SIZE]u8 = undefined;
        var fr = Io.File.Reader.init(file, io, &file_buf);
        const freader = &fr.interface;

        // 读取第一行: 记录总数
        const first_line = freader.takeDelimiter('\n') catch {
            self.expense_count = 0;
            return false;
        };
        if (first_line == null) {
            self.expense_count = 0;
            return false;
        }
        const trimmed_first = std.mem.trim(u8, first_line.?, " \t\r\n");
        const count_from_file = std.fmt.parseInt(usize, trimmed_first, 10) catch {
            self.expense_count = 0;
            return false;
        };
        if (count_from_file > MAX_EXPENSES) {
            self.expense_count = 0;
            return false;
        }

        var loaded_count: usize = 0;
        var stderr_buf: [256]u8 = undefined;
        var stderr_fw = Io.File.Writer.initStreaming(.stderr(), io, &stderr_buf);
        const stderr_w = &stderr_fw.interface;

        for (0..count_from_file) |i| {
            const raw_line = freader.takeDelimiter('\n') catch break;
            if (raw_line == null) break;
            const line = std.mem.trimEnd(u8, raw_line.?, "\r");

            // 解析CSV: year,month,day,description,amount,category
            var rest = line;

            // 年份
            const year_sep = std.mem.indexOfScalar(u8, rest, ',') orelse {
                stderr_w.print("警告：记录 {d} 数据不完整 (年份)。\n", .{i + 1}) catch {};
                continue;
            };
            const year = std.fmt.parseInt(i32, rest[0..year_sep], 10) catch {
                stderr_w.print("警告：无效年份格式在记录 {d}。跳过此记录。\n", .{i + 1}) catch {};
                continue;
            };
            rest = rest[year_sep + 1 ..];

            // 月份
            const month_sep = std.mem.indexOfScalar(u8, rest, ',') orelse {
                stderr_w.print("警告：记录 {d} 数据不完整 (月份)。\n", .{i + 1}) catch {};
                continue;
            };
            const month_val = std.fmt.parseInt(i32, rest[0..month_sep], 10) catch {
                stderr_w.print("警告：无效月份格式在记录 {d}。跳过此记录。\n", .{i + 1}) catch {};
                continue;
            };
            rest = rest[month_sep + 1 ..];

            // 日期
            const day_sep = std.mem.indexOfScalar(u8, rest, ',') orelse {
                stderr_w.print("警告：记录 {d} 数据不完整 (日期)。\n", .{i + 1}) catch {};
                continue;
            };
            const day_val = std.fmt.parseInt(i32, rest[0..day_sep], 10) catch {
                stderr_w.print("警告：无效日期格式在记录 {d}。跳过此记录。\n", .{i + 1}) catch {};
                continue;
            };
            rest = rest[day_sep + 1 ..];

            // 描述
            const desc_sep = std.mem.indexOfScalar(u8, rest, ',') orelse {
                stderr_w.print("警告：记录 {d} 数据不完整 (描述)。\n", .{i + 1}) catch {};
                continue;
            };
            var description = rest[0..desc_sep];
            if (description.len > MAX_DESCRIPTION_LENGTH) {
                description = description[0..MAX_DESCRIPTION_LENGTH];
            }
            rest = rest[desc_sep + 1 ..];

            // 金额
            const amt_sep = std.mem.indexOfScalar(u8, rest, ',') orelse {
                stderr_w.print("警告：记录 {d} 数据不完整 (金额)。\n", .{i + 1}) catch {};
                continue;
            };
            const amount_val = std.fmt.parseFloat(f64, rest[0..amt_sep]) catch {
                stderr_w.print("警告：无效金额格式在记录 {d}。跳过此记录。\n", .{i + 1}) catch {};
                continue;
            };
            rest = rest[amt_sep + 1 ..];

            // 类别 (行尾剩余部分)
            var category = rest;
            if (category.len > MAX_CATEGORY_LENGTH) {
                category = category[0..MAX_CATEGORY_LENGTH];
            }

            if (loaded_count < MAX_EXPENSES) {
                self.all_expenses[loaded_count].setData(year, month_val, day_val, description, amount_val, category);
                loaded_count += 1;
            } else {
                break;
            }
        }
        stderr_w.flush() catch {};

        self.expense_count = loaded_count;
        return true;
    }

    // ─── 读取上次结算状态 ───
    fn readLastSettlement(io: Io) struct { year: i32, month: i32 } {
        const file = Io.Dir.openFile(.cwd(), io, SETTLEMENT_FILE, .{}) catch {
            return .{ .year = 0, .month = 0 };
        };
        defer file.close(io);
        var file_buf: [64]u8 = undefined;
        var fr = Io.File.Reader.init(file, io, &file_buf);
        const freader = &fr.interface;

        const line = freader.takeDelimiter('\n') catch {
            return .{ .year = 0, .month = 0 };
        };
        if (line == null) return .{ .year = 0, .month = 0 };
        const trimmed = std.mem.trim(u8, line.?, " \t\r\n");

        // 格式: "year month"
        var it = std.mem.splitScalar(u8, trimmed, ' ');
        const year_str = it.next() orelse return .{ .year = 0, .month = 0 };
        const month_str = it.next() orelse return .{ .year = 0, .month = 0 };
        const y = std.fmt.parseInt(i32, year_str, 10) catch return .{ .year = 0, .month = 0 };
        const m = std.fmt.parseInt(i32, month_str, 10) catch return .{ .year = 0, .month = 0 };
        return .{ .year = y, .month = m };
    }

    // ─── 写入结算状态 ───
    fn writeLastSettlement(io: Io, year: i32, month: i32) !void {
        const file = Io.Dir.createFile(.cwd(), io, SETTLEMENT_FILE, .{}) catch {
            var stderr_buf: [256]u8 = undefined;
            var stderr_fw = Io.File.Writer.initStreaming(.stderr(), io, &stderr_buf);
            stderr_fw.interface.print("错误：无法写入结算状态文件 {s}\n", .{SETTLEMENT_FILE}) catch {};
            stderr_fw.interface.flush() catch {};
            return;
        };
        defer file.close(io);
        var file_buf: [64]u8 = undefined;
        var fw = Io.File.Writer.initStreaming(file, io, &file_buf);
        try fw.interface.print("{d} {d}\n", .{ year, month });
        try fw.interface.flush();
    }

    // ─── 自动结算 ───
    fn performAutomaticSettlement(self: *ExpenseTracker) !void {
        const io = self.io;
        const w = self.writer();
        var settlement = readLastSettlement(io);
        var last_settled_year = settlement.year;
        var last_settled_month = settlement.month;

        const today = getCurrentDate();
        const current_year = today.year;
        const current_month = today.month;

        // 首次运行
        if (last_settled_year == 0) {
            last_settled_year = current_year;
            last_settled_month = current_month;
            if (last_settled_month == 1) {
                last_settled_month = 12;
                last_settled_year -= 1;
            } else {
                last_settled_month -= 1;
            }
            try writeLastSettlement(io, last_settled_year, last_settled_month);
            const m_u32: u32 = @intCast(if (last_settled_month >= 0) @as(u32, @intCast(last_settled_month)) else 0);
            try w.print("首次运行或无结算记录，已设置基准结算点为: {d}年{d:0>2}月。\n", .{ last_settled_year, m_u32 });
            try self.flushStdout();
            return;
        }

        // 逐月结算
        var year_to_settle = last_settled_year;
        var month_to_settle = last_settled_month;

        while (true) {
            month_to_settle += 1;
            if (month_to_settle > 12) {
                month_to_settle = 1;
                year_to_settle += 1;
            }

            if (year_to_settle > current_year or (year_to_settle == current_year and month_to_settle >= current_month)) {
                break;
            }

            const m_u32: u32 = @intCast(if (month_to_settle >= 0) @as(u32, @intCast(month_to_settle)) else 0);
            try w.print("\n>>> 开始自动结算: {d}年{d:0>2}月 <<\n", .{ year_to_settle, m_u32 });
            try self.printMonthlyReport(year_to_settle, month_to_settle, true);
            try writeLastSettlement(io, year_to_settle, month_to_settle);
            try w.print(">>> 自动结算完成: {d}年{d:0>2}月 <<\n", .{ year_to_settle, m_u32 });
            try self.flushStdout();
        }
    }

    // ─── 删除开销 ───
    fn deleteExpense(self: *ExpenseTracker) !void {
        const w = self.writer();
        if (self.expense_count == 0) {
            try w.print("没有开销记录可供删除。\n", .{});
            return;
        }

        try w.print("\n--- 删除开销记录 ---\n", .{});
        try w.print("以下是所有开销记录:\n", .{});

        // 打印带序号的表头
        try padRight(w, "序号", 5);
        try padRight(w, "日期", 12);
        try padRight(w, "描述", 30);
        try padRight(w, "类别", 20);
        try padLeft(w, "金额", 10);
        try w.writeByte('\n');
        try writeRepeat(w, '-', 77);
        try w.writeByte('\n');

        for (0..self.expense_count) |i| {
            // 序号
            var num_buf: [8]u8 = undefined;
            var num_w: Io.Writer = .fixed(&num_buf);
            num_w.print("{d}", .{i + 1}) catch {};
            const num_str = num_w.buffered();
            try padRight(w, num_str, 5);
            try printExpenseRow(w, &self.all_expenses[i]);
        }
        try writeRepeat(w, '-', 77);
        try w.writeByte('\n');
        try self.flushStdout();

        // 获取要删除的序号
        try w.print("请输入要删除的记录序号 (0 取消删除): ", .{});
        try self.flushStdout();
        var record_number: i32 = -1;
        while (true) {
            if (try readLine(self.reader())) |line| {
                if (parseInt(line)) |v| {
                    if (v >= 0 and v <= @as(i32, @intCast(self.expense_count))) {
                        record_number = v;
                        break;
                    }
                }
            }
            try w.print("输入无效。请输入 1 到 {d} 之间的数字，或 0 取消: ", .{self.expense_count});
            try self.flushStdout();
        }

        if (record_number == 0) {
            try w.print("取消删除操作。\n", .{});
            return;
        }

        const index_to_delete: usize = @intCast(record_number - 1);

        // 显示要删除的记录
        try w.print("\n即将删除以下记录:\n", .{});
        try printTableHeader(w);
        try printExpenseRow(w, &self.all_expenses[index_to_delete]);
        try writeRepeat(w, '-', 72);
        try w.writeByte('\n');
        try self.flushStdout();

        // 第一次确认
        try w.print("确认删除吗？ (y/n): ", .{});
        try self.flushStdout();
        if (try readLine(self.reader())) |line| {
            const trimmed = std.mem.trim(u8, line, " \t\r\n");
            if (trimmed.len > 0 and (trimmed[0] == 'y' or trimmed[0] == 'Y')) {
                // 第二次确认
                try w.print("\n警告：此操作无法撤销！\n", .{});
                try w.print("最后一次确认，真的要删除这条记录吗？ (y/n): ", .{});
                try self.flushStdout();
                if (try readLine(self.reader())) |line2| {
                    const trimmed2 = std.mem.trim(u8, line2, " \t\r\n");
                    if (trimmed2.len > 0 and (trimmed2[0] == 'y' or trimmed2[0] == 'Y')) {
                        try w.print("\n正在删除记录...\n", .{});
                        // 将后续元素前移
                        var idx = index_to_delete;
                        while (idx < self.expense_count - 1) : (idx += 1) {
                            self.all_expenses[idx] = self.all_expenses[idx + 1];
                        }
                        self.expense_count -= 1;
                        try w.print("记录已删除。\n", .{});
                        try self.saveExpenses();
                        try w.print("数据已自动保存。\n", .{});
                    } else {
                        try w.print("已取消删除操作（二次确认未通过）。\n", .{});
                    }
                }
            } else {
                try w.print("取消删除操作。\n", .{});
            }
        }
    }
};

// ─── 主函数 ───
pub fn main(init: std.process.Init) !void {
    var tracker = ExpenseTracker.create(init.io);
    try tracker.init();
    try tracker.run();
}

#![allow(unused_unsafe)]

use std::alloc::System;
use std::fs;
use std::io::{self, BufRead, BufReader, BufWriter, Write};
use std::time::SystemTime;

// Use system allocator to avoid jemalloc overhead and reduce startup time
#[global_allocator]
static GLOBAL: System = System;

const MAX_EXPENSES: usize = 1000;
const MAX_UNIQUE_CATEGORIES_PER_MONTH: usize = 20;
const DATA_FILE: &str = "expenses.dat";
const SETTLEMENT_FILE: &str = "settlement_status.txt";
const MAX_DESCRIPTION_LENGTH: usize = 100;
const MAX_CATEGORY_LENGTH: usize = 50;

// Buffer sizes tuned for cache hierarchy: 64KB for I/O buffers
const IO_BUF_SIZE: usize = 65536;

// Stack-allocated fixed-size string buffer to avoid heap allocations
const DESC_BUF_CAP: usize = 104; // slightly over MAX_DESCRIPTION_LENGTH for safety
const CAT_BUF_CAP: usize = 54;   // slightly over MAX_CATEGORY_LENGTH for safety

/// Fixed-capacity stack string to avoid heap allocations for short strings.
/// Stored inline in the Expense struct.
#[derive(Clone)]
struct FixedStr<const N: usize> {
    buf: [u8; N],
    len: u16,
}

impl<const N: usize> FixedStr<N> {
    #[inline(always)]
    const fn new() -> Self {
        FixedStr {
            buf: [0u8; N],
            len: 0,
        }
    }

    #[inline(always)]
    fn from_str(s: &str) -> Self {
        let bytes = s.as_bytes();
        let copy_len = if bytes.len() > N { N } else { bytes.len() };
        let mut buf = [0u8; N];
        // Manual copy to avoid bounds checks in hot path
        unsafe {
            std::ptr::copy_nonoverlapping(bytes.as_ptr(), buf.as_mut_ptr(), copy_len);
        }
        FixedStr {
            buf,
            len: copy_len as u16,
        }
    }

    #[inline(always)]
    fn as_str(&self) -> &str {
        unsafe { std::str::from_utf8_unchecked(&self.buf[..self.len as usize]) }
    }

    #[inline(always)]
    #[allow(dead_code)]
    fn is_empty(&self) -> bool {
        self.len == 0
    }
}

impl<const N: usize> std::fmt::Display for FixedStr<N> {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.as_str())
    }
}

impl<const N: usize> PartialEq for FixedStr<N> {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}

impl<const N: usize> PartialEq<str> for FixedStr<N> {
    #[inline(always)]
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}

/// Packed Expense struct using smaller integer types.
/// Aligned to cache line boundary for optimal memory access patterns.
#[repr(C)]
struct Expense {
    amount: f64,               // 8 bytes - put first for alignment
    description: FixedStr<DESC_BUF_CAP>, // 106 bytes
    category: FixedStr<CAT_BUF_CAP>,     // 56 bytes
    year: i16,                 // 2 bytes (was i32)
    month: i8,                 // 1 byte (was i32)
    day: i8,                   // 1 byte (was i32)
}

impl Expense {
    #[inline(always)]
    const fn new() -> Self {
        Expense {
            year: 0,
            month: 0,
            day: 0,
            description: FixedStr::new(),
            amount: 0.0,
            category: FixedStr::new(),
        }
    }

    #[inline(always)]
    fn set_data(&mut self, y: i32, m: i32, d: i32, desc: &str, amt: f64, cat: &str) {
        self.year = y as i16;
        self.month = m as i8;
        self.day = d as i8;
        self.description = FixedStr::from_str(desc);
        self.amount = amt;
        self.category = FixedStr::from_str(cat);
    }
}

struct CategorySum {
    name: FixedStr<CAT_BUF_CAP>,
    total: f64,
}

impl CategorySum {
    #[inline(always)]
    fn new() -> Self {
        CategorySum {
            name: FixedStr::new(),
            total: 0.0,
        }
    }
}

struct ExpenseTracker {
    all_expenses: Vec<Expense>,
    expense_count: usize,
}

#[inline(always)]
fn get_current_date() -> (i32, i32, i32) {
    let now = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;

    unsafe {
        let time_val = now as libc::time_t;
        let tm = libc::localtime(&time_val);
        let year = (*tm).tm_year + 1900;
        let month = (*tm).tm_mon + 1;
        let day = (*tm).tm_mday;
        (year, month, day)
    }
}

/// Read a line from the locked BufReader, trimming trailing newlines.
#[inline(always)]
fn read_line_from(reader: &mut BufReader<io::StdinLock<'_>>, buf: &mut String) {
    buf.clear();
    let _ = reader.read_line(buf);
    if buf.ends_with('\n') {
        buf.pop();
        if buf.ends_with('\r') {
            buf.pop();
        }
    }
}

/// Parse an integer from the locked BufReader.
#[inline(always)]
fn read_int_from(reader: &mut BufReader<io::StdinLock<'_>>, buf: &mut String) -> Option<i32> {
    read_line_from(reader, buf);
    buf.trim().parse::<i32>().ok()
}

/// Write expense table header (without index column) to the BufWriter.
#[inline(always)]
fn write_expense_header(w: &mut BufWriter<io::StdoutLock<'_>>) {
    let _ = writeln!(
        w,
        "{:<12}{:<30}{:<20}{:>10}",
        "日期", "描述", "类别", "金额"
    );
    // Pre-computed 72-dash separator
    let _ = writeln!(w, "------------------------------------------------------------------------");
}

/// Write expense table header (with index column) to the BufWriter.
#[inline(always)]
fn write_expense_header_with_index(w: &mut BufWriter<io::StdoutLock<'_>>) {
    let _ = writeln!(
        w,
        "{:<5}{:<12}{:<30}{:<20}{:>10}",
        "序号", "日期", "描述", "类别", "金额"
    );
    // Pre-computed 77-dash separator
    let _ = writeln!(w, "-----------------------------------------------------------------------------");
}

/// Write a single expense row to the BufWriter.
#[inline(always)]
fn write_expense_row(w: &mut BufWriter<io::StdoutLock<'_>>, exp: &Expense) {
    let _ = writeln!(
        w,
        "{:<4}-{:02}-{:02}  {:<30}{:<20}{:>10.2}",
        exp.year, exp.month, exp.day, exp.description, exp.category, exp.amount
    );
}

/// Write a single expense row with index to the BufWriter.
#[inline(always)]
fn write_expense_row_with_index(w: &mut BufWriter<io::StdoutLock<'_>>, index: usize, exp: &Expense) {
    let _ = writeln!(
        w,
        "{:<5}{:<4}-{:02}-{:02}  {:<30}{:<20}{:>10.2}",
        index, exp.year, exp.month, exp.day, exp.description, exp.category, exp.amount
    );
}

// Pre-computed separator constants
const DASH_72: &str = "------------------------------------------------------------------------";
const DASH_77: &str = "-----------------------------------------------------------------------------";
const DASH_30: &str = "------------------------------";

impl ExpenseTracker {
    fn new(
        w: &mut BufWriter<io::StdoutLock<'_>>,
    ) -> Self {
        let mut tracker = ExpenseTracker {
            all_expenses: Vec::with_capacity(MAX_EXPENSES),
            expense_count: 0,
        };
        // Pre-allocate all expense slots
        for _ in 0..MAX_EXPENSES {
            tracker.all_expenses.push(Expense::new());
        }

        if tracker.load_expenses() {
            let _ = writeln!(w, "成功加载 {} 条历史记录。", tracker.expense_count);
        } else {
            let _ = writeln!(w, "未找到历史数据文件或加载失败，开始新的记录。");
        }
        tracker.perform_automatic_settlement(w);
        tracker
    }

    fn run(
        &mut self,
        r: &mut BufReader<io::StdinLock<'_>>,
        w: &mut BufWriter<io::StdoutLock<'_>>,
    ) {
        let mut buf = String::with_capacity(256);
        loop {
            let _ = writeln!(w, "\n大学生开销追踪器");
            let _ = writeln!(w, "--------------------");
            let _ = writeln!(w, "1. 添加开销记录");
            let _ = writeln!(w, "2. 查看所有开销");
            let _ = writeln!(w, "3. 查看月度统计");
            let _ = writeln!(w, "4. 按期间列出开销");
            let _ = writeln!(w, "5. 删除开销记录");
            let _ = writeln!(w, "6. 保存并退出");
            let _ = writeln!(w, "--------------------");
            let _ = write!(w, "请输入选项: ");
            let _ = w.flush();

            let choice = read_int_from(r, &mut buf).unwrap_or(0);

            match choice {
                1 => self.add_expense(r, w, &mut buf),
                2 => self.display_all_expenses(w),
                3 => self.display_monthly_summary(r, w, &mut buf),
                4 => self.list_expenses_by_period(r, w, &mut buf),
                5 => self.delete_expense(r, w, &mut buf),
                6 => {
                    self.save_expenses();
                    let _ = writeln!(w, "数据已保存。正在退出...");
                    let _ = w.flush();
                }
                _ => {
                    let _ = writeln!(w, "无效选项，请重试。");
                }
            }

            if choice == 6 {
                break;
            }
        }
    }

    fn add_expense(
        &mut self,
        r: &mut BufReader<io::StdinLock<'_>>,
        w: &mut BufWriter<io::StdoutLock<'_>>,
        buf: &mut String,
    ) {
        if self.expense_count >= MAX_EXPENSES {
            let _ = writeln!(w, "错误：开销记录已满！无法添加更多记录。");
            return;
        }

        let _ = writeln!(w, "\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---");

        let (current_year, current_month, current_day) = get_current_date();

        // Get year
        let _ = write!(w, "输入年份 (YYYY) [默认: {}, -1 取消]: ", current_year);
        let _ = w.flush();
        read_line_from(r, buf);
        if buf.as_str() == "-1" {
            let _ = writeln!(w, "已取消添加开销。");
            return;
        }
        let year = if !buf.is_empty() {
            match buf.trim().parse::<i32>() {
                Ok(y) => y,
                Err(_) => {
                    let _ = writeln!(
                        w,
                        "年份输入无效或包含非数字字符，将使用默认年份: {}。",
                        current_year
                    );
                    current_year
                }
            }
        } else {
            current_year
        };

        // Get month
        let _ = write!(w, "输入月份 (MM) [默认: {}, -1 取消]: ", current_month);
        let _ = w.flush();
        read_line_from(r, buf);
        if buf.as_str() == "-1" {
            let _ = writeln!(w, "已取消添加开销。");
            return;
        }
        let month = if !buf.is_empty() {
            match buf.trim().parse::<i32>() {
                Ok(m) if m >= 1 && m <= 12 => m,
                _ => {
                    let _ = writeln!(
                        w,
                        "月份输入无效或范围不正确 (1-12)，将使用默认月份: {}。",
                        current_month
                    );
                    current_month
                }
            }
        } else {
            current_month
        };

        // Get day
        let _ = write!(w, "输入日期 (DD) [默认: {}, -1 取消]: ", current_day);
        let _ = w.flush();
        read_line_from(r, buf);
        if buf.as_str() == "-1" {
            let _ = writeln!(w, "已取消添加开销。");
            return;
        }
        let day = if !buf.is_empty() {
            match buf.trim().parse::<i32>() {
                Ok(d) if d >= 1 && d <= 31 => d,
                _ => {
                    let _ = writeln!(
                        w,
                        "日期输入无效或范围不正确 (1-31)，将使用默认日期: {}。",
                        current_day
                    );
                    current_day
                }
            }
        } else {
            current_day
        };

        // Basic date validation
        if month < 1 || month > 12 || day < 1 || day > 31 {
            let _ = writeln!(w, "日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。");
            return;
        }

        // Get description
        let _ = write!(
            w,
            "输入描述 (最多 {} 字符, 输入 '!cancel' 取消): ",
            MAX_DESCRIPTION_LENGTH
        );
        let _ = w.flush();
        read_line_from(r, buf);
        if buf.as_str() == "!cancel" {
            let _ = writeln!(w, "已取消添加开销。");
            return;
        }
        let mut description = buf.clone();
        if description.len() > MAX_DESCRIPTION_LENGTH {
            let _ = writeln!(w, "描述过长，已截断为 {} 字符。", MAX_DESCRIPTION_LENGTH);
            let mut end = MAX_DESCRIPTION_LENGTH;
            while end < description.len() && !description.is_char_boundary(end) {
                end += 1;
            }
            if end > MAX_DESCRIPTION_LENGTH {
                end = MAX_DESCRIPTION_LENGTH;
                while end > 0 && !description.is_char_boundary(end) {
                    end -= 1;
                }
            }
            description.truncate(end);
        }

        // Get amount
        let _ = write!(w, "输入金额 (-1 取消): ");
        let _ = w.flush();
        let amount: f64;
        loop {
            read_line_from(r, buf);
            if buf.as_str() == "-1" {
                let _ = writeln!(w, "已取消添加开销。");
                return;
            }
            match buf.trim().parse::<f64>() {
                Ok(a) if a >= 0.0 => {
                    amount = a;
                    break;
                }
                _ => {
                    let _ = write!(w, "金额无效或为负，请重新输入 (-1 取消): ");
                    let _ = w.flush();
                }
            }
        }

        // Get category
        let _ = write!(
            w,
            "输入类别 (如 餐饮, 交通, 娱乐; 最多 {} 字符, 输入 '!cancel' 取消): ",
            MAX_CATEGORY_LENGTH
        );
        let _ = w.flush();
        read_line_from(r, buf);
        if buf.as_str() == "!cancel" {
            let _ = writeln!(w, "已取消添加开销。");
            return;
        }
        let mut category = buf.clone();
        if category.len() > MAX_CATEGORY_LENGTH {
            let _ = writeln!(w, "类别名称过长，已截断为 {} 字符。", MAX_CATEGORY_LENGTH);
            let mut end = MAX_CATEGORY_LENGTH;
            while end > 0 && !category.is_char_boundary(end) {
                end -= 1;
            }
            category.truncate(end);
        }
        if category.is_empty() {
            category = "未分类".to_string();
        }

        unsafe {
            self.all_expenses
                .get_unchecked_mut(self.expense_count)
                .set_data(year, month, day, &description, amount, &category);
        }
        self.expense_count += 1;
        let _ = writeln!(w, "开销已添加。");
    }

    fn display_all_expenses(&self, w: &mut BufWriter<io::StdoutLock<'_>>) {
        if self.expense_count == 0 {
            let _ = writeln!(w, "没有开销记录。");
            return;
        }
        let _ = writeln!(w, "\n--- 所有开销记录 ---");
        write_expense_header(w);

        for i in 0..self.expense_count {
            unsafe {
                write_expense_row(w, self.all_expenses.get_unchecked(i));
            }
        }
        let _ = writeln!(w, "{}", DASH_72);
        let _ = w.flush();
    }

    fn display_monthly_summary(
        &self,
        r: &mut BufReader<io::StdinLock<'_>>,
        w: &mut BufWriter<io::StdoutLock<'_>>,
        buf: &mut String,
    ) {
        let _ = writeln!(w, "\n--- 月度开销统计 ---");

        // Get year
        let _ = write!(w, "输入要统计的年份 (YYYY) (-1 取消): ");
        let _ = w.flush();
        let year: i32;
        loop {
            match read_int_from(r, buf) {
                Some(-1) => {
                    let _ = writeln!(w, "已取消月度统计。");
                    return;
                }
                Some(y) => {
                    year = y;
                    break;
                }
                None => {
                    let _ = write!(w, "年份输入无效，请重新输入 (-1 取消): ");
                    let _ = w.flush();
                }
            }
        }

        // Get month
        let _ = write!(w, "输入要统计的月份 (MM) (-1 取消): ");
        let _ = w.flush();
        let month: i32;
        loop {
            match read_int_from(r, buf) {
                Some(-1) => {
                    let _ = writeln!(w, "已取消月度统计。");
                    return;
                }
                Some(m) if m >= 1 && m <= 12 => {
                    month = m;
                    break;
                }
                Some(_) => {
                    let _ = write!(w, "月份输入无效 (1-12)，请重新输入 (-1 取消): ");
                    let _ = w.flush();
                }
                None => {
                    let _ = write!(w, "月份输入无效 (1-12)，请重新输入 (-1 取消): ");
                    let _ = w.flush();
                }
            }
        }

        let _ = writeln!(w, "\n--- {}年{:02}月 开销统计 ---", year, month);

        let mut total_month_amount: f64 = 0.0;
        let mut found_records = false;

        // Stack-allocated category sums array to avoid heap allocation
        let mut category_sums: [CategorySum; MAX_UNIQUE_CATEGORIES_PER_MONTH] =
            std::array::from_fn(|_| CategorySum::new());
        let mut category_count: usize = 0;
        #[allow(unused_variables, unused_mut)]
        let mut max_category_total: f64 = 0.0;

        write_expense_header(w);

        let year_i16 = year as i16;
        let month_i8 = month as i8;

        for i in 0..self.expense_count {
            let exp = unsafe { self.all_expenses.get_unchecked(i) };
            if exp.year == year_i16 && exp.month == month_i8 {
                found_records = true;
                write_expense_row(w, exp);
                total_month_amount += exp.amount;

                // Category aggregation
                let mut category_exists = false;
                for j in 0..category_count {
                    let cs = unsafe { category_sums.get_unchecked_mut(j) };
                    if cs.name == exp.category {
                        cs.total += exp.amount;
                        category_exists = true;
                        if cs.total > max_category_total {
                            max_category_total = cs.total;
                        }
                        break;
                    }
                }
                if !category_exists && category_count < MAX_UNIQUE_CATEGORIES_PER_MONTH {
                    let cs = unsafe { category_sums.get_unchecked_mut(category_count) };
                    cs.name = exp.category.clone();
                    cs.total = exp.amount;
                    if cs.total > max_category_total {
                        max_category_total = cs.total;
                    }
                    category_count += 1;
                }
            }
        }

        if !found_records {
            let _ = writeln!(w, "该月份没有开销记录。");
        } else {
            let _ = writeln!(w, "{}", DASH_72);
            let _ = writeln!(w, "{:<62}{:>10.2}", "本月总计:", total_month_amount);
            let _ = writeln!(w);

            if category_count > 0 {
                let _ = writeln!(w, "按类别汇总:");
                let _ = writeln!(w, "{:<20}{:>10}", "类别", "总金额");
                let _ = writeln!(w, "{}", DASH_30);
                for j in 0..category_count {
                    let cs = unsafe { category_sums.get_unchecked(j) };
                    let _ = writeln!(w, "{:<20}{:>10.2}", cs.name, cs.total);
                }
                let _ = writeln!(w, "{}", DASH_30);
            }
        }
        let _ = w.flush();
    }

    fn list_expenses_by_period(
        &self,
        r: &mut BufReader<io::StdinLock<'_>>,
        w: &mut BufWriter<io::StdoutLock<'_>>,
        buf: &mut String,
    ) {
        loop {
            let _ = writeln!(w, "\n--- 按期间列出开销 --- ");
            let _ = writeln!(w, "1. 按年份列出");
            let _ = writeln!(w, "2. 按月份列出");
            let _ = writeln!(w, "3. 按日期列出");
            let _ = writeln!(w, "0. 返回主菜单");
            let _ = writeln!(w, "--------------------");
            let _ = write!(w, "请输入选项: ");
            let _ = w.flush();

            let choice = read_int_from(r, buf).unwrap_or(-1);

            match choice {
                1 => {
                    let _ = writeln!(w, "\n--- 按年份列出开销 ---");
                    let _ = write!(w, "输入年份 (YYYY) (输入 0 返回): ");
                    let _ = w.flush();
                    let year: i32;
                    loop {
                        match read_int_from(r, buf) {
                            Some(y) => {
                                year = y;
                                break;
                            }
                            None => {
                                let _ = write!(w, "年份输入无效，请重新输入 (输入 0 返回): ");
                                let _ = w.flush();
                            }
                        }
                    }
                    if year == 0 {
                        continue;
                    }

                    let year_i16 = year as i16;
                    let mut found = false;
                    write_expense_header(w);
                    for i in 0..self.expense_count {
                        let exp = unsafe { self.all_expenses.get_unchecked(i) };
                        if exp.year == year_i16 {
                            write_expense_row(w, exp);
                            found = true;
                        }
                    }
                    if !found {
                        let _ = writeln!(w, "在 {} 年没有找到开销记录。", year);
                    }
                    let _ = writeln!(w, "{}", DASH_72);
                    let _ = w.flush();
                }
                2 => {
                    let _ = writeln!(w, "\n--- 按月份列出开销 ---");
                    let _ = write!(w, "输入年份 (YYYY) (输入 0 返回): ");
                    let _ = w.flush();
                    let year: i32;
                    loop {
                        match read_int_from(r, buf) {
                            Some(y) => {
                                year = y;
                                break;
                            }
                            None => {
                                let _ = write!(w, "年份输入无效，请重新输入 (输入 0 返回): ");
                                let _ = w.flush();
                            }
                        }
                    }
                    if year == 0 {
                        continue;
                    }

                    let _ = write!(w, "输入月份 (MM) (输入 0 返回): ");
                    let _ = w.flush();
                    let month: i32;
                    loop {
                        match read_int_from(r, buf) {
                            Some(m) if m == 0 || (m >= 1 && m <= 12) => {
                                month = m;
                                break;
                            }
                            _ => {
                                let _ = write!(w, "月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                                let _ = w.flush();
                            }
                        }
                    }
                    if month == 0 {
                        continue;
                    }

                    let year_i16 = year as i16;
                    let month_i8 = month as i8;
                    let mut found = false;
                    write_expense_header(w);
                    for i in 0..self.expense_count {
                        let exp = unsafe { self.all_expenses.get_unchecked(i) };
                        if exp.year == year_i16 && exp.month == month_i8 {
                            write_expense_row(w, exp);
                            found = true;
                        }
                    }
                    if !found {
                        let _ = writeln!(w, "在 {} 年 {} 月没有找到开销记录。", year, month);
                    }
                    let _ = writeln!(w, "{}", DASH_72);
                    let _ = w.flush();
                }
                3 => {
                    let _ = writeln!(w, "\n--- 按日期列出开销 ---");
                    let _ = write!(w, "输入年份 (YYYY) (输入 0 返回): ");
                    let _ = w.flush();
                    let year: i32;
                    loop {
                        match read_int_from(r, buf) {
                            Some(y) => {
                                year = y;
                                break;
                            }
                            None => {
                                let _ = write!(w, "年份输入无效，请重新输入 (输入 0 返回): ");
                                let _ = w.flush();
                            }
                        }
                    }
                    if year == 0 {
                        continue;
                    }

                    let _ = write!(w, "输入月份 (MM) (输入 0 返回): ");
                    let _ = w.flush();
                    let month: i32;
                    loop {
                        match read_int_from(r, buf) {
                            Some(m) if m == 0 || (m >= 1 && m <= 12) => {
                                month = m;
                                break;
                            }
                            _ => {
                                let _ = write!(w, "月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                                let _ = w.flush();
                            }
                        }
                    }
                    if month == 0 {
                        continue;
                    }

                    let _ = write!(w, "输入日期 (DD) (输入 0 返回): ");
                    let _ = w.flush();
                    let day: i32;
                    loop {
                        match read_int_from(r, buf) {
                            Some(d) if d == 0 || (d >= 1 && d <= 31) => {
                                day = d;
                                break;
                            }
                            _ => {
                                let _ = write!(w, "日期输入无效 (1-31)，请重新输入 (输入 0 返回): ");
                                let _ = w.flush();
                            }
                        }
                    }
                    if day == 0 {
                        continue;
                    }

                    let year_i16 = year as i16;
                    let month_i8 = month as i8;
                    let day_i8 = day as i8;
                    let mut found = false;
                    write_expense_header(w);
                    for i in 0..self.expense_count {
                        let exp = unsafe { self.all_expenses.get_unchecked(i) };
                        if exp.year == year_i16 && exp.month == month_i8 && exp.day == day_i8 {
                            write_expense_row(w, exp);
                            found = true;
                        }
                    }
                    if !found {
                        let _ = writeln!(
                            w,
                            "在 {} 年 {} 月 {} 日没有找到开销记录。",
                            year, month, day
                        );
                    }
                    let _ = writeln!(w, "{}", DASH_72);
                    let _ = w.flush();
                }
                0 => {
                    let _ = writeln!(w, "返回主菜单...");
                }
                _ => {
                    let _ = writeln!(w, "无效选项，请重试。");
                }
            }

            if choice == 0 {
                break;
            }
        }
    }

    fn save_expenses(&self) {
        // Pre-allocate a String with estimated capacity to avoid reallocations
        // Each line is roughly: "YYYY,MM,DD,desc,amount,cat\n" ~ 200 bytes max
        let estimated_size = 16 + self.expense_count * 200;
        let mut content = String::with_capacity(estimated_size);
        content.push_str(&format!("{}\n", self.expense_count));
        for i in 0..self.expense_count {
            let exp = unsafe { self.all_expenses.get_unchecked(i) };
            content.push_str(&format!(
                "{},{},{},{},{},{}\n",
                exp.year, exp.month, exp.day, exp.description, exp.amount, exp.category
            ));
        }
        if let Err(_) = fs::write(DATA_FILE, &content) {
            eprintln!("错误：无法打开文件 {} 进行写入！", DATA_FILE);
        }
    }

    fn load_expenses(&mut self) -> bool {
        let content = match fs::read_to_string(DATA_FILE) {
            Ok(c) => c,
            Err(_) => return false,
        };

        let mut lines = content.lines();

        // Read count from first line
        let count_from_file: usize = match lines.next() {
            Some(line) => match line.trim().parse::<usize>() {
                Ok(c) if c <= MAX_EXPENSES => c,
                _ => {
                    self.expense_count = 0;
                    return false;
                }
            },
            None => {
                self.expense_count = 0;
                return false;
            }
        };

        let mut loaded_count: usize = 0;

        for (i, line) in lines.enumerate() {
            if i >= count_from_file {
                break;
            }
            if loaded_count >= MAX_EXPENSES {
                break;
            }

            let segments: Vec<&str> = line.splitn(6, ',').collect();

            // Parse year
            let year: i32 = match segments.get(0) {
                Some(s) => match s.parse() {
                    Ok(v) => v,
                    Err(_) => {
                        eprintln!(
                            "警告：无效年份格式 '{}' 在记录 {}。跳过此记录。",
                            s,
                            i + 1
                        );
                        continue;
                    }
                },
                None => {
                    eprintln!("警告：记录 {} 数据不完整 (年份)。", i + 1);
                    continue;
                }
            };

            // Parse month
            let month: i32 = match segments.get(1) {
                Some(s) => match s.parse() {
                    Ok(v) => v,
                    Err(_) => {
                        eprintln!(
                            "警告：无效月份格式 '{}' 在记录 {}。跳过此记录。",
                            s,
                            i + 1
                        );
                        continue;
                    }
                },
                None => {
                    eprintln!("警告：记录 {} 数据不完整 (月份)。", i + 1);
                    continue;
                }
            };

            // Parse day
            let day: i32 = match segments.get(2) {
                Some(s) => match s.parse() {
                    Ok(v) => v,
                    Err(_) => {
                        eprintln!(
                            "警告：无效日期格式 '{}' 在记录 {}。跳过此记录。",
                            s,
                            i + 1
                        );
                        continue;
                    }
                },
                None => {
                    eprintln!("警告：记录 {} 数据不完整 (日期)。", i + 1);
                    continue;
                }
            };

            // Parse description
            let description_str: &str = match segments.get(3) {
                Some(s) => {
                    let desc = *s;
                    if desc.len() > MAX_DESCRIPTION_LENGTH {
                        let mut end = MAX_DESCRIPTION_LENGTH;
                        while end > 0 && !desc.is_char_boundary(end) {
                            end -= 1;
                        }
                        &desc[..end]
                    } else {
                        desc
                    }
                }
                None => {
                    eprintln!("警告：记录 {} 数据不完整 (描述)。", i + 1);
                    continue;
                }
            };

            // Parse amount
            let amount: f64 = match segments.get(4) {
                Some(s) => match s.parse() {
                    Ok(v) => v,
                    Err(_) => {
                        eprintln!(
                            "警告：无效金额格式 '{}' 在记录 {}。跳过此记录。",
                            s,
                            i + 1
                        );
                        continue;
                    }
                },
                None => {
                    eprintln!("警告：记录 {} 数据不完整 (金额)。", i + 1);
                    continue;
                }
            };

            // Parse category
            let category_str: &str = match segments.get(5) {
                Some(s) => {
                    let cat = *s;
                    if cat.len() > MAX_CATEGORY_LENGTH {
                        let mut end = MAX_CATEGORY_LENGTH;
                        while end > 0 && !cat.is_char_boundary(end) {
                            end -= 1;
                        }
                        &cat[..end]
                    } else {
                        cat
                    }
                }
                None => "",
            };

            unsafe {
                self.all_expenses.get_unchecked_mut(loaded_count).set_data(
                    year,
                    month,
                    day,
                    description_str,
                    amount,
                    category_str,
                );
            }
            loaded_count += 1;
        }

        self.expense_count = loaded_count;
        true
    }

    #[inline(always)]
    fn read_last_settlement(&self) -> (i32, i32) {
        let mut last_year: i32 = 0;
        let mut last_month: i32 = 0;
        if let Ok(content) = fs::read_to_string(SETTLEMENT_FILE) {
            let parts: Vec<&str> = content.trim().split_whitespace().collect();
            if let Some(y) = parts.get(0) {
                last_year = y.parse().unwrap_or(0);
            }
            if let Some(m) = parts.get(1) {
                last_month = m.parse().unwrap_or(0);
            }
        }
        (last_year, last_month)
    }

    #[inline(always)]
    fn write_last_settlement(&self, year: i32, month: i32) {
        if let Err(_) = fs::write(SETTLEMENT_FILE, format!("{} {}\n", year, month)) {
            eprintln!("错误：无法写入结算状态文件 {}", SETTLEMENT_FILE);
        }
    }

    fn generate_monthly_report_for_settlement(
        &self,
        w: &mut BufWriter<io::StdoutLock<'_>>,
        year: i32,
        month: i32,
    ) {
        let _ = writeln!(
            w,
            "\n--- {}年{:02}月 开销报告 (自动结算) ---",
            year, month
        );

        let mut total_month_amount: f64 = 0.0;
        let mut found_records = false;
        let mut category_sums: [CategorySum; MAX_UNIQUE_CATEGORIES_PER_MONTH] =
            std::array::from_fn(|_| CategorySum::new());
        let mut category_count: usize = 0;
        #[allow(unused_variables, unused_mut)]
        let mut max_category_total: f64 = 0.0;

        let _ = writeln!(w, "明细:");
        write_expense_header(w);

        let year_i16 = year as i16;
        let month_i8 = month as i8;

        for i in 0..self.expense_count {
            let exp = unsafe { self.all_expenses.get_unchecked(i) };
            if exp.year == year_i16 && exp.month == month_i8 {
                found_records = true;
                write_expense_row(w, exp);
                total_month_amount += exp.amount;

                let mut category_exists = false;
                for j in 0..category_count {
                    let cs = unsafe { category_sums.get_unchecked_mut(j) };
                    if cs.name == exp.category {
                        cs.total += exp.amount;
                        category_exists = true;
                        if cs.total > max_category_total {
                            max_category_total = cs.total;
                        }
                        break;
                    }
                }
                if !category_exists && category_count < MAX_UNIQUE_CATEGORIES_PER_MONTH {
                    let cs = unsafe { category_sums.get_unchecked_mut(category_count) };
                    cs.name = exp.category.clone();
                    cs.total = exp.amount;
                    if cs.total > max_category_total {
                        max_category_total = cs.total;
                    }
                    category_count += 1;
                }
            }
        }

        if !found_records {
            let _ = writeln!(w, "该月份没有开销记录。");
            return;
        }

        let _ = writeln!(w, "{}", DASH_72);
        let _ = writeln!(w, "{:<62}{:>10.2}", "本月总计:", total_month_amount);
        let _ = writeln!(w);

        if category_count > 0 {
            let _ = writeln!(w, "按类别汇总:");
            let _ = writeln!(w, "{:<20}{:>10}", "类别", "总金额");
            let _ = writeln!(w, "{}", DASH_30);
            for j in 0..category_count {
                let cs = unsafe { category_sums.get_unchecked(j) };
                let _ = writeln!(w, "{:<20}{:>10.2}", cs.name, cs.total);
            }
            let _ = writeln!(w, "{}", DASH_30);
        }

        let _ = writeln!(w, "--- 报告生成完毕 ---");
    }

    fn perform_automatic_settlement(&self, w: &mut BufWriter<io::StdoutLock<'_>>) {
        let (mut last_settled_year, mut last_settled_month) = self.read_last_settlement();
        let (current_year, current_month, _) = get_current_date();

        if last_settled_year == 0 {
            last_settled_year = current_year;
            last_settled_month = current_month;
            if last_settled_month == 1 {
                last_settled_month = 12;
                last_settled_year -= 1;
            } else {
                last_settled_month -= 1;
            }
            self.write_last_settlement(last_settled_year, last_settled_month);
            let _ = writeln!(
                w,
                "首次运行或无结算记录，已设置基准结算点为: {}年{:02}月。",
                last_settled_year, last_settled_month
            );
            return;
        }

        let mut year_to_settle = last_settled_year;
        let mut month_to_settle = last_settled_month;

        loop {
            month_to_settle += 1;
            if month_to_settle > 12 {
                month_to_settle = 1;
                year_to_settle += 1;
            }

            if year_to_settle > current_year
                || (year_to_settle == current_year && month_to_settle >= current_month)
            {
                break;
            }

            let _ = writeln!(
                w,
                "\n>>> 开始自动结算: {}年{:02}月 <<",
                year_to_settle, month_to_settle
            );
            self.generate_monthly_report_for_settlement(w, year_to_settle, month_to_settle);
            self.write_last_settlement(year_to_settle, month_to_settle);
            let _ = writeln!(
                w,
                ">>> 自动结算完成: {}年{:02}月 <<",
                year_to_settle, month_to_settle
            );
        }
    }

    fn delete_expense(
        &mut self,
        r: &mut BufReader<io::StdinLock<'_>>,
        w: &mut BufWriter<io::StdoutLock<'_>>,
        buf: &mut String,
    ) {
        if self.expense_count == 0 {
            let _ = writeln!(w, "没有开销记录可供删除。");
            return;
        }

        let _ = writeln!(w, "\n--- 删除开销记录 ---");
        let _ = writeln!(w, "以下是所有开销记录:");
        write_expense_header_with_index(w);

        for i in 0..self.expense_count {
            unsafe {
                write_expense_row_with_index(w, i + 1, self.all_expenses.get_unchecked(i));
            }
        }
        let _ = writeln!(w, "{}", DASH_77);

        // Get record number to delete
        let _ = write!(w, "请输入要删除的记录序号 (0 取消删除): ");
        let _ = w.flush();
        let record_number: usize;
        loop {
            match read_int_from(r, buf) {
                Some(n) if n >= 0 && (n as usize) <= self.expense_count => {
                    record_number = n as usize;
                    break;
                }
                _ => {
                    let _ = write!(
                        w,
                        "输入无效。请输入 1 到 {} 之间的数字，或 0 取消: ",
                        self.expense_count
                    );
                    let _ = w.flush();
                }
            }
        }

        if record_number == 0 {
            let _ = writeln!(w, "取消删除操作。");
            return;
        }

        let index_to_delete = record_number - 1;

        let _ = writeln!(w, "\n即将删除以下记录:");
        write_expense_header(w);
        write_expense_row(w, &self.all_expenses[index_to_delete]);
        let _ = writeln!(w, "{}", DASH_72);

        // First confirmation
        let _ = write!(w, "确认删除吗？ (y/n): ");
        let _ = w.flush();
        read_line_from(r, buf);

        if buf.starts_with('y') || buf.starts_with('Y') {
            // Second confirmation
            let _ = writeln!(w, "\n警告：此操作无法撤销！");
            let _ = write!(w, "最后一次确认，真的要删除这条记录吗？ (y/n): ");
            let _ = w.flush();
            read_line_from(r, buf);

            if buf.starts_with('y') || buf.starts_with('Y') {
                let _ = writeln!(w, "\n正在删除记录...");

                // Shift elements left - use direct field copies to avoid String cloning
                for i in index_to_delete..self.expense_count - 1 {
                    let (year, month, day, desc, amount, cat) = {
                        let next = unsafe { self.all_expenses.get_unchecked(i + 1) };
                        (
                            next.year,
                            next.month,
                            next.day,
                            next.description.clone(),
                            next.amount,
                            next.category.clone(),
                        )
                    };
                    let current = unsafe { self.all_expenses.get_unchecked_mut(i) };
                    current.year = year;
                    current.month = month;
                    current.day = day;
                    current.description = desc;
                    current.amount = amount;
                    current.category = cat;
                }
                self.expense_count -= 1;
                let _ = writeln!(w, "记录已删除。");
                self.save_expenses();
                let _ = writeln!(w, "数据已自动保存。");
            } else {
                let _ = writeln!(w, "已取消删除操作（二次确认未通过）。");
            }
        } else {
            let _ = writeln!(w, "取消删除操作。");
        }
        let _ = w.flush();
    }
}

fn main() {
    // Lock stdout and stdin once for the entire program lifetime, wrap in large buffers
    let stdout = io::stdout();
    let stdout_lock = stdout.lock();
    let mut w = BufWriter::with_capacity(IO_BUF_SIZE, stdout_lock);

    let stdin = io::stdin();
    let stdin_lock = stdin.lock();
    let mut r = BufReader::with_capacity(IO_BUF_SIZE, stdin_lock);

    let mut tracker = ExpenseTracker::new(&mut w);
    let _ = w.flush();
    tracker.run(&mut r, &mut w);
}

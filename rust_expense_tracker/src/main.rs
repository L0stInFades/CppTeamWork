use std::io::{self, BufRead, Write};
use std::fs;
use std::time::SystemTime;

const MAX_EXPENSES: usize = 1000;
const MAX_UNIQUE_CATEGORIES_PER_MONTH: usize = 20;
const DATA_FILE: &str = "expenses.dat";
const SETTLEMENT_FILE: &str = "settlement_status.txt";
const MAX_DESCRIPTION_LENGTH: usize = 100;
const MAX_CATEGORY_LENGTH: usize = 50;

struct Expense {
    year: i32,
    month: i32,
    day: i32,
    description: String,
    amount: f64,
    category: String,
}

impl Expense {
    fn new() -> Self {
        Expense {
            year: 0,
            month: 0,
            day: 0,
            description: String::new(),
            amount: 0.0,
            category: String::new(),
        }
    }

    fn set_data(&mut self, y: i32, m: i32, d: i32, desc: &str, amt: f64, cat: &str) {
        self.year = y;
        self.month = m;
        self.day = d;
        self.description = desc.to_string();
        self.amount = amt;
        self.category = cat.to_string();
    }
}

struct CategorySum {
    name: String,
    total: f64,
}

impl CategorySum {
    fn new() -> Self {
        CategorySum {
            name: String::new(),
            total: 0.0,
        }
    }
}

struct ExpenseTracker {
    all_expenses: Vec<Expense>,
    expense_count: usize,
}

fn get_current_date() -> (i32, i32, i32) {
    // Use libc localtime to match the C++ behavior
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

fn read_line() -> String {
    let mut line = String::new();
    io::stdin().lock().read_line(&mut line).unwrap_or(0);
    if line.ends_with('\n') {
        line.pop();
        if line.ends_with('\r') {
            line.pop();
        }
    }
    line
}

fn read_int() -> Option<i32> {
    let line = read_line();
    line.trim().parse::<i32>().ok()
}

fn flush_stdout() {
    io::stdout().flush().unwrap_or(());
}

/// Print expense table header (without index column)
fn print_expense_header() {
    println!(
        "{:<12}{:<30}{:<20}{:>10}",
        "日期", "描述", "类别", "金额"
    );
    println!("{}", "-".repeat(72));
}

/// Print expense table header (with index column)
fn print_expense_header_with_index() {
    println!(
        "{:<5}{:<12}{:<30}{:<20}{:>10}",
        "序号", "日期", "描述", "类别", "金额"
    );
    println!("{}", "-".repeat(77));
}

/// Print a single expense row
fn print_expense_row(exp: &Expense) {
    println!(
        "{:<4}-{:02}-{:02}  {:<30}{:<20}{:>10.2}",
        exp.year, exp.month, exp.day, exp.description, exp.category, exp.amount
    );
}

/// Print a single expense row with index
fn print_expense_row_with_index(index: usize, exp: &Expense) {
    println!(
        "{:<5}{:<4}-{:02}-{:02}  {:<30}{:<20}{:>10.2}",
        index, exp.year, exp.month, exp.day, exp.description, exp.category, exp.amount
    );
}

impl ExpenseTracker {
    fn new() -> Self {
        let mut tracker = ExpenseTracker {
            all_expenses: Vec::new(),
            expense_count: 0,
        };
        // Initialize with empty expenses
        for _ in 0..MAX_EXPENSES {
            tracker.all_expenses.push(Expense::new());
        }

        if tracker.load_expenses() {
            println!("成功加载 {} 条历史记录。", tracker.expense_count);
        } else {
            println!("未找到历史数据文件或加载失败，开始新的记录。");
        }
        tracker.perform_automatic_settlement();
        tracker
    }

    fn run(&mut self) {
        let mut choice: i32;
        loop {
            println!("\n大学生开销追踪器");
            println!("--------------------");
            println!("1. 添加开销记录");
            println!("2. 查看所有开销");
            println!("3. 查看月度统计");
            println!("4. 按期间列出开销");
            println!("5. 删除开销记录");
            println!("6. 保存并退出");
            println!("--------------------");
            print!("请输入选项: ");
            flush_stdout();

            choice = read_int().unwrap_or(0);

            match choice {
                1 => self.add_expense(),
                2 => self.display_all_expenses(),
                3 => self.display_monthly_summary(),
                4 => self.list_expenses_by_period(),
                5 => self.delete_expense(),
                6 => {
                    self.save_expenses();
                    println!("数据已保存。正在退出...");
                }
                _ => println!("无效选项，请重试。"),
            }

            if choice == 6 {
                break;
            }
        }
    }

    fn add_expense(&mut self) {
        if self.expense_count >= MAX_EXPENSES {
            println!("错误：开销记录已满！无法添加更多记录。");
            return;
        }

        println!("\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---");

        let (current_year, current_month, current_day) = get_current_date();

        // Get year
        print!("输入年份 (YYYY) [默认: {}, -1 取消]: ", current_year);
        flush_stdout();
        let line_input = read_line();
        if line_input == "-1" {
            println!("已取消添加开销。");
            return;
        }
        let year = if !line_input.is_empty() {
            match line_input.trim().parse::<i32>() {
                Ok(y) => y,
                Err(_) => {
                    println!(
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
        print!("输入月份 (MM) [默认: {}, -1 取消]: ", current_month);
        flush_stdout();
        let line_input = read_line();
        if line_input == "-1" {
            println!("已取消添加开销。");
            return;
        }
        let month = if !line_input.is_empty() {
            match line_input.trim().parse::<i32>() {
                Ok(m) if m >= 1 && m <= 12 => m,
                _ => {
                    println!(
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
        print!("输入日期 (DD) [默认: {}, -1 取消]: ", current_day);
        flush_stdout();
        let line_input = read_line();
        if line_input == "-1" {
            println!("已取消添加开销。");
            return;
        }
        let day = if !line_input.is_empty() {
            match line_input.trim().parse::<i32>() {
                Ok(d) if d >= 1 && d <= 31 => d,
                _ => {
                    println!(
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
            println!("日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。");
            return;
        }

        // Get description
        print!(
            "输入描述 (最多 {} 字符, 输入 '!cancel' 取消): ",
            MAX_DESCRIPTION_LENGTH
        );
        flush_stdout();
        let mut description = read_line();
        if description == "!cancel" {
            println!("已取消添加开销。");
            return;
        }
        if description.len() > MAX_DESCRIPTION_LENGTH {
            println!("描述过长，已截断为 {} 字符。", MAX_DESCRIPTION_LENGTH);
            // Truncate at char boundary
            let mut end = MAX_DESCRIPTION_LENGTH;
            while end < description.len() && !description.is_char_boundary(end) {
                end += 1;
            }
            if end > MAX_DESCRIPTION_LENGTH {
                // Try to find the last valid char boundary within limit
                end = MAX_DESCRIPTION_LENGTH;
                while end > 0 && !description.is_char_boundary(end) {
                    end -= 1;
                }
            }
            description.truncate(end);
        }

        // Get amount
        print!("输入金额 (-1 取消): ");
        flush_stdout();
        let amount: f64;
        loop {
            let line_input = read_line();
            if line_input == "-1" {
                println!("已取消添加开销。");
                return;
            }
            match line_input.trim().parse::<f64>() {
                Ok(a) if a >= 0.0 => {
                    amount = a;
                    break;
                }
                _ => {
                    print!("金额无效或为负，请重新输入 (-1 取消): ");
                    flush_stdout();
                }
            }
        }

        // Get category
        print!(
            "输入类别 (如 餐饮, 交通, 娱乐; 最多 {} 字符, 输入 '!cancel' 取消): ",
            MAX_CATEGORY_LENGTH
        );
        flush_stdout();
        let mut category = read_line();
        if category == "!cancel" {
            println!("已取消添加开销。");
            return;
        }
        if category.len() > MAX_CATEGORY_LENGTH {
            println!("类别名称过长，已截断为 {} 字符。", MAX_CATEGORY_LENGTH);
            let mut end = MAX_CATEGORY_LENGTH;
            while end > 0 && !category.is_char_boundary(end) {
                end -= 1;
            }
            category.truncate(end);
        }
        if category.is_empty() {
            category = "未分类".to_string();
        }

        self.all_expenses[self.expense_count].set_data(year, month, day, &description, amount, &category);
        self.expense_count += 1;
        println!("开销已添加。");
    }

    fn display_all_expenses(&self) {
        if self.expense_count == 0 {
            println!("没有开销记录。");
            return;
        }
        println!("\n--- 所有开销记录 ---");
        print_expense_header();

        for i in 0..self.expense_count {
            print_expense_row(&self.all_expenses[i]);
        }
        println!("{}", "-".repeat(72));
    }

    fn display_monthly_summary(&self) {
        println!("\n--- 月度开销统计 ---");

        // Get year
        print!("输入要统计的年份 (YYYY) (-1 取消): ");
        flush_stdout();
        let year: i32;
        loop {
            match read_int() {
                Some(-1) => {
                    println!("已取消月度统计。");
                    return;
                }
                Some(y) => {
                    year = y;
                    break;
                }
                None => {
                    print!("年份输入无效，请重新输入 (-1 取消): ");
                    flush_stdout();
                }
            }
        }

        // Get month
        print!("输入要统计的月份 (MM) (-1 取消): ");
        flush_stdout();
        let month: i32;
        loop {
            match read_int() {
                Some(-1) => {
                    println!("已取消月度统计。");
                    return;
                }
                Some(m) if m >= 1 && m <= 12 => {
                    month = m;
                    break;
                }
                Some(_) => {
                    print!("月份输入无效 (1-12)，请重新输入 (-1 取消): ");
                    flush_stdout();
                }
                None => {
                    print!("月份输入无效 (1-12)，请重新输入 (-1 取消): ");
                    flush_stdout();
                }
            }
        }

        println!("\n--- {}年{:02}月 开销统计 ---", year, month);

        let mut total_month_amount: f64 = 0.0;
        let mut found_records = false;

        let mut category_sums: Vec<CategorySum> = Vec::new();
        #[allow(unused_variables, unused_mut)]
        let mut max_category_total: f64 = 0.0;

        print_expense_header();

        for i in 0..self.expense_count {
            let exp = &self.all_expenses[i];
            if exp.year == year && exp.month == month {
                found_records = true;
                print_expense_row(exp);
                total_month_amount += exp.amount;

                // Category aggregation
                let mut category_exists = false;
                for cs in category_sums.iter_mut() {
                    if cs.name == exp.category {
                        cs.total += exp.amount;
                        category_exists = true;
                        if cs.total > max_category_total {
                            max_category_total = cs.total;
                        }
                        break;
                    }
                }
                if !category_exists && category_sums.len() < MAX_UNIQUE_CATEGORIES_PER_MONTH {
                    let mut cs = CategorySum::new();
                    cs.name = exp.category.clone();
                    cs.total = exp.amount;
                    if cs.total > max_category_total {
                        max_category_total = cs.total;
                    }
                    category_sums.push(cs);
                }
            }
        }

        if !found_records {
            println!("该月份没有开销记录。");
        } else {
            println!("{}", "-".repeat(72));
            println!("{:<62}{:>10.2}", "本月总计:", total_month_amount);
            println!();

            if !category_sums.is_empty() {
                println!("按类别汇总:");
                println!("{:<20}{:>10}", "类别", "总金额");
                println!("{}", "-".repeat(30));
                for cs in &category_sums {
                    println!("{:<20}{:>10.2}", cs.name, cs.total);
                }
                println!("{}", "-".repeat(30));
            }
        }
    }

    fn list_expenses_by_period(&self) {
        let mut choice: i32;
        loop {
            println!("\n--- 按期间列出开销 --- ");
            println!("1. 按年份列出");
            println!("2. 按月份列出");
            println!("3. 按日期列出");
            println!("0. 返回主菜单");
            println!("--------------------");
            print!("请输入选项: ");
            flush_stdout();

            choice = read_int().unwrap_or(-1);

            match choice {
                1 => {
                    println!("\n--- 按年份列出开销 ---");
                    print!("输入年份 (YYYY) (输入 0 返回): ");
                    flush_stdout();
                    let year: i32;
                    loop {
                        match read_int() {
                            Some(y) => {
                                year = y;
                                break;
                            }
                            None => {
                                print!("年份输入无效，请重新输入 (输入 0 返回): ");
                                flush_stdout();
                            }
                        }
                    }
                    if year == 0 {
                        continue;
                    }

                    let mut found = false;
                    print_expense_header();
                    for i in 0..self.expense_count {
                        if self.all_expenses[i].year == year {
                            print_expense_row(&self.all_expenses[i]);
                            found = true;
                        }
                    }
                    if !found {
                        println!("在 {} 年没有找到开销记录。", year);
                    }
                    println!("{}", "-".repeat(72));
                }
                2 => {
                    println!("\n--- 按月份列出开销 ---");
                    print!("输入年份 (YYYY) (输入 0 返回): ");
                    flush_stdout();
                    let year: i32;
                    loop {
                        match read_int() {
                            Some(y) => {
                                year = y;
                                break;
                            }
                            None => {
                                print!("年份输入无效，请重新输入 (输入 0 返回): ");
                                flush_stdout();
                            }
                        }
                    }
                    if year == 0 {
                        continue;
                    }

                    print!("输入月份 (MM) (输入 0 返回): ");
                    flush_stdout();
                    let month: i32;
                    loop {
                        match read_int() {
                            Some(m) if m == 0 || (m >= 1 && m <= 12) => {
                                month = m;
                                break;
                            }
                            _ => {
                                print!("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                                flush_stdout();
                            }
                        }
                    }
                    if month == 0 {
                        continue;
                    }

                    let mut found = false;
                    print_expense_header();
                    for i in 0..self.expense_count {
                        let exp = &self.all_expenses[i];
                        if exp.year == year && exp.month == month {
                            print_expense_row(exp);
                            found = true;
                        }
                    }
                    if !found {
                        println!("在 {} 年 {} 月没有找到开销记录。", year, month);
                    }
                    println!("{}", "-".repeat(72));
                }
                3 => {
                    println!("\n--- 按日期列出开销 ---");
                    print!("输入年份 (YYYY) (输入 0 返回): ");
                    flush_stdout();
                    let year: i32;
                    loop {
                        match read_int() {
                            Some(y) => {
                                year = y;
                                break;
                            }
                            None => {
                                print!("年份输入无效，请重新输入 (输入 0 返回): ");
                                flush_stdout();
                            }
                        }
                    }
                    if year == 0 {
                        continue;
                    }

                    print!("输入月份 (MM) (输入 0 返回): ");
                    flush_stdout();
                    let month: i32;
                    loop {
                        match read_int() {
                            Some(m) if m == 0 || (m >= 1 && m <= 12) => {
                                month = m;
                                break;
                            }
                            _ => {
                                print!("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ");
                                flush_stdout();
                            }
                        }
                    }
                    if month == 0 {
                        continue;
                    }

                    print!("输入日期 (DD) (输入 0 返回): ");
                    flush_stdout();
                    let day: i32;
                    loop {
                        match read_int() {
                            Some(d) if d == 0 || (d >= 1 && d <= 31) => {
                                day = d;
                                break;
                            }
                            _ => {
                                print!("日期输入无效 (1-31)，请重新输入 (输入 0 返回): ");
                                flush_stdout();
                            }
                        }
                    }
                    if day == 0 {
                        continue;
                    }

                    let mut found = false;
                    print_expense_header();
                    for i in 0..self.expense_count {
                        let exp = &self.all_expenses[i];
                        if exp.year == year && exp.month == month && exp.day == day {
                            print_expense_row(exp);
                            found = true;
                        }
                    }
                    if !found {
                        println!(
                            "在 {} 年 {} 月 {} 日没有找到开销记录。",
                            year, month, day
                        );
                    }
                    println!("{}", "-".repeat(72));
                }
                0 => {
                    println!("返回主菜单...");
                }
                _ => {
                    println!("无效选项，请重试。");
                }
            }

            if choice == 0 {
                break;
            }
        }
    }

    fn save_expenses(&self) {
        let mut content = String::new();
        content.push_str(&format!("{}\n", self.expense_count));
        for i in 0..self.expense_count {
            let exp = &self.all_expenses[i];
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
            let description_str = match segments.get(3) {
                Some(s) => {
                    let mut desc = s.to_string();
                    if desc.len() > MAX_DESCRIPTION_LENGTH {
                        let mut end = MAX_DESCRIPTION_LENGTH;
                        while end > 0 && !desc.is_char_boundary(end) {
                            end -= 1;
                        }
                        desc.truncate(end);
                    }
                    desc
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
            let category_str = match segments.get(5) {
                Some(s) => {
                    let mut cat = s.to_string();
                    if cat.len() > MAX_CATEGORY_LENGTH {
                        let mut end = MAX_CATEGORY_LENGTH;
                        while end > 0 && !cat.is_char_boundary(end) {
                            end -= 1;
                        }
                        cat.truncate(end);
                    }
                    cat
                }
                None => String::new(),
            };

            self.all_expenses[loaded_count].set_data(
                year,
                month,
                day,
                &description_str,
                amount,
                &category_str,
            );
            loaded_count += 1;
        }

        self.expense_count = loaded_count;
        true
    }

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

    fn write_last_settlement(&self, year: i32, month: i32) {
        if let Err(_) = fs::write(SETTLEMENT_FILE, format!("{} {}\n", year, month)) {
            eprintln!("错误：无法写入结算状态文件 {}", SETTLEMENT_FILE);
        }
    }

    fn generate_monthly_report_for_settlement(&self, year: i32, month: i32) {
        println!(
            "\n--- {}年{:02}月 开销报告 (自动结算) ---",
            year, month
        );

        let mut total_month_amount: f64 = 0.0;
        let mut found_records = false;
        let mut category_sums: Vec<CategorySum> = Vec::new();
        #[allow(unused_variables, unused_mut)]
        let mut max_category_total: f64 = 0.0;

        println!("明细:");
        print_expense_header();

        for i in 0..self.expense_count {
            let exp = &self.all_expenses[i];
            if exp.year == year && exp.month == month {
                found_records = true;
                print_expense_row(exp);
                total_month_amount += exp.amount;

                let mut category_exists = false;
                for cs in category_sums.iter_mut() {
                    if cs.name == exp.category {
                        cs.total += exp.amount;
                        category_exists = true;
                        if cs.total > max_category_total {
                            max_category_total = cs.total;
                        }
                        break;
                    }
                }
                if !category_exists && category_sums.len() < MAX_UNIQUE_CATEGORIES_PER_MONTH {
                    let mut cs = CategorySum::new();
                    cs.name = exp.category.clone();
                    cs.total = exp.amount;
                    if cs.total > max_category_total {
                        max_category_total = cs.total;
                    }
                    category_sums.push(cs);
                }
            }
        }

        if !found_records {
            println!("该月份没有开销记录。");
            return;
        }

        println!("{}", "-".repeat(72));
        println!("{:<62}{:>10.2}", "本月总计:", total_month_amount);
        println!();

        if !category_sums.is_empty() {
            println!("按类别汇总:");
            println!("{:<20}{:>10}", "类别", "总金额");
            println!("{}", "-".repeat(30));
            for cs in &category_sums {
                println!("{:<20}{:>10.2}", cs.name, cs.total);
            }
            println!("{}", "-".repeat(30));
        }

        println!("--- 报告生成完毕 ---");
    }

    fn perform_automatic_settlement(&self) {
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
            println!(
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

            println!(
                "\n>>> 开始自动结算: {}年{:02}月 <<",
                year_to_settle, month_to_settle
            );
            self.generate_monthly_report_for_settlement(year_to_settle, month_to_settle);
            self.write_last_settlement(year_to_settle, month_to_settle);
            println!(
                ">>> 自动结算完成: {}年{:02}月 <<",
                year_to_settle, month_to_settle
            );
        }
    }

    fn delete_expense(&mut self) {
        if self.expense_count == 0 {
            println!("没有开销记录可供删除。");
            return;
        }

        println!("\n--- 删除开销记录 ---");
        println!("以下是所有开销记录:");
        print_expense_header_with_index();

        for i in 0..self.expense_count {
            print_expense_row_with_index(i + 1, &self.all_expenses[i]);
        }
        println!("{}", "-".repeat(77));

        // Get record number to delete
        print!("请输入要删除的记录序号 (0 取消删除): ");
        flush_stdout();
        let record_number: usize;
        loop {
            match read_int() {
                Some(n) if n >= 0 && (n as usize) <= self.expense_count => {
                    record_number = n as usize;
                    break;
                }
                _ => {
                    print!(
                        "输入无效。请输入 1 到 {} 之间的数字，或 0 取消: ",
                        self.expense_count
                    );
                    flush_stdout();
                }
            }
        }

        if record_number == 0 {
            println!("取消删除操作。");
            return;
        }

        let index_to_delete = record_number - 1;

        println!("\n即将删除以下记录:");
        print_expense_header();
        print_expense_row(&self.all_expenses[index_to_delete]);
        println!("{}", "-".repeat(72));

        // First confirmation
        print!("确认删除吗？ (y/n): ");
        flush_stdout();
        let confirm = read_line();

        if confirm.starts_with('y') || confirm.starts_with('Y') {
            // Second confirmation
            println!("\n警告：此操作无法撤销！");
            print!("最后一次确认，真的要删除这条记录吗？ (y/n): ");
            flush_stdout();
            let final_confirm = read_line();

            if final_confirm.starts_with('y') || final_confirm.starts_with('Y') {
                println!("\n正在删除记录...");

                for i in index_to_delete..self.expense_count - 1 {
                    // Shift elements left
                    let (year, month, day, desc, amount, cat) = {
                        let next = &self.all_expenses[i + 1];
                        (
                            next.year,
                            next.month,
                            next.day,
                            next.description.clone(),
                            next.amount,
                            next.category.clone(),
                        )
                    };
                    self.all_expenses[i].set_data(year, month, day, &desc, amount, &cat);
                }
                self.expense_count -= 1;
                println!("记录已删除。");
                self.save_expenses();
                println!("数据已自动保存。");
            } else {
                println!("已取消删除操作（二次确认未通过）。");
            }
        } else {
            println!("取消删除操作。");
        }
    }
}

fn main() {
    let mut tracker = ExpenseTracker::new();
    tracker.run();
}

// Optimized Expense Tracker
// Compile with: g++ -O2 -march=native -mtune=native -flto main.cpp -o expense_tracker_cpp
// Also compiles with: g++ -O2 main.cpp -o expense_tracker_cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <limits>

using namespace std;

// stdout buffer for full buffering
static char stdout_buf[1 << 16];

// Struct for expense - cache-line friendly packing
struct Expense {
	double amount;
	int year;
	int month;
	int day;
	char description[104]; // MAX_DESCRIPTION_LENGTH=100 + padding
	char category[52];     // MAX_CATEGORY_LENGTH=50 + padding

	static const size_t MAX_DESCRIPTION_LENGTH = 100;
	static const size_t MAX_CATEGORY_LENGTH = 50;

	Expense() : amount(0.0), year(0), month(0), day(0) {
		description[0] = '\0';
		category[0] = '\0';
	}

	void setData(int y, int m, int d, const string& desc, double amt, const string& cat) {
		year = y; month = m; day = d; amount = amt;
		size_t dlen = desc.size();
		if (dlen > MAX_DESCRIPTION_LENGTH) dlen = MAX_DESCRIPTION_LENGTH;
		memcpy(description, desc.c_str(), dlen);
		description[dlen] = '\0';
		size_t clen = cat.size();
		if (clen > MAX_CATEGORY_LENGTH) clen = MAX_CATEGORY_LENGTH;
		memcpy(category, cat.c_str(), clen);
		category[clen] = '\0';
	}

	void setDataRaw(int y, int m, int d, const char* desc, size_t dlen, double amt, const char* cat, size_t clen) {
		year = y; month = m; day = d; amount = amt;
		if (dlen > MAX_DESCRIPTION_LENGTH) dlen = MAX_DESCRIPTION_LENGTH;
		memcpy(description, desc, dlen);
		description[dlen] = '\0';
		if (clen > MAX_CATEGORY_LENGTH) clen = MAX_CATEGORY_LENGTH;
		memcpy(category, cat, clen);
		category[clen] = '\0';
	}

	int getYear() const { return year; }
	int getMonth() const { return month; }
	int getDay() const { return day; }
	const char* getDescription() const { return description; }
	double getAmount() const { return amount; }
	const char* getCategory() const { return category; }
};

struct CategorySum {
	char name[52];
	double total;
	CategorySum() : total(0.0) { name[0] = '\0'; }
};

static const int MAX_EXPENSES = 1000;
static const int MAX_UNIQUE_CATEGORIES_PER_MONTH = 20;
static const char* DATA_FILE = "expenses.dat";
static const char* SETTLEMENT_FILE = "settlement_status.txt";

// Print the standard table header (no sequence number)
static inline void print_table_header() {
	// "日期" left in 12, "描述" left in 30, "类别" left in 20, "金额" right in 10
	// Use cout for the header to match original iomanip formatting exactly
	cout << left
		 << setw(12) << "日期"
		 << setw(30) << "描述"
		 << setw(20) << "类别"
		 << right << setw(10) << "金额\n";
}

// Print separator line of given width
static inline void print_separator(int width) {
	cout << string(width, '-') << "\n";
}

// Print one expense record in standard format using cout (for exact formatting match)
static inline void print_expense_record(const Expense& e) {
	cout << left
		 << setw(4) << e.getYear() << "-"
		 << setfill('0') << setw(2) << e.getMonth() << "-"
		 << setw(2) << e.getDay()
		 << setfill(' ') << "  "
		 << setw(30) << e.getDescription()
		 << setw(20) << e.getCategory()
		 << right << fixed << setprecision(2) << setw(10) << e.getAmount() << "\n";
}

class ExpenseTracker {
private:
	Expense allExpenses[MAX_EXPENSES];
	int expenseCount;

	void clearInputBuffer() {
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
	}

	void readLastSettlement(int& lastYear, int& lastMonth);
	void writeLastSettlement(int year, int month);
	void generateMonthlyReportForSettlement(int year, int month);

public:
	ExpenseTracker();
	~ExpenseTracker() {}

	void run();
	void addExpense();
	void displayAllExpenses();
	void displayMonthlySummary();
	void listExpensesByPeriod();
	void saveExpenses();
	bool loadExpenses();
	void deleteExpense();
	void performAutomaticSettlement();
};

ExpenseTracker::ExpenseTracker() : expenseCount(0) {
	if (loadExpenses()) {
		cout << "成功加载 " << expenseCount << " 条历史记录。\n";
	} else {
		cout << "未找到历史数据文件或加载失败，开始新的记录。\n";
	}
	performAutomaticSettlement();
}

void ExpenseTracker::run() {
	int choice;
	do {
		cout << "\n大学生开销追踪器\n";
		cout << "--------------------\n";
		cout << "1. 添加开销记录\n";
		cout << "2. 查看所有开销\n";
		cout << "3. 查看月度统计\n";
		cout << "4. 按期间列出开销\n";
		cout << "5. 删除开销记录\n";
		cout << "6. 保存并退出\n";
		cout << "--------------------\n";
		cout << "请输入选项: ";

		cin >> choice;

		if (__builtin_expect(cin.fail(), 0)) {
			cin.clear();
			clearInputBuffer();
			choice = 0;
		} else {
			clearInputBuffer();
		}

		switch (choice) {
		case 1:
			addExpense();
			break;
		case 2:
			displayAllExpenses();
			break;
		case 3:
			displayMonthlySummary();
			break;
		case 4:
			listExpensesByPeriod();
			break;
		case 5:
			deleteExpense();
			break;
		case 6:
			saveExpenses();
			cout << "数据已保存。正在退出...\n";
			break;
		default:
			cout << "无效选项，请重试。\n";
		}
	} while (choice != 6);
}

void ExpenseTracker::addExpense() {
	if (__builtin_expect(expenseCount >= MAX_EXPENSES, 0)) {
		cout << "错误：开销记录已满！无法添加更多记录。\n";
		return;
	}

	int year, month, day;
	string description;
	double amount;
	string category;
	string line_input;

	cout << "\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---\n";

	time_t now = time(0);
	tm *ltm = localtime(&now);
	int currentYear = 1900 + ltm->tm_year;
	int currentMonth = 1 + ltm->tm_mon;
	int currentDay = ltm->tm_mday;

	// Year input
	cout << "输入年份 (YYYY) [默认: " << currentYear << ", -1 取消]: ";
	getline(cin, line_input);
	if (line_input == "-1") { cout << "已取消添加开销。\n"; return; }
	if (!line_input.empty()) {
		stringstream ss(line_input);
		if (!(ss >> year) || !ss.eof()) {
			cout << "年份输入无效或包含非数字字符，将使用默认年份: " << currentYear << "。\n";
			year = currentYear;
		}
	} else {
		year = currentYear;
	}

	// Month input
	cout << "输入月份 (MM) [默认: " << currentMonth << ", -1 取消]: ";
	getline(cin, line_input);
	if (line_input == "-1") { cout << "已取消添加开销。\n"; return; }
	if (!line_input.empty()) {
		stringstream ss(line_input);
		if (!(ss >> month) || !ss.eof() || month < 1 || month > 12) {
			cout << "月份输入无效或范围不正确 (1-12)，将使用默认月份: " << currentMonth << "。\n";
			month = currentMonth;
		}
	} else {
		month = currentMonth;
	}

	// Day input
	cout << "输入日期 (DD) [默认: " << currentDay << ", -1 取消]: ";
	getline(cin, line_input);
	if (line_input == "-1") { cout << "已取消添加开销。\n"; return; }
	if (!line_input.empty()) {
		stringstream ss(line_input);
		if (!(ss >> day) || !ss.eof() || day < 1 || day > 31) {
			cout << "日期输入无效或范围不正确 (1-31)，将使用默认日期: " << currentDay << "。\n";
			day = currentDay;
		}
	} else {
		day = currentDay;
	}

	if (month < 1 || month > 12 || day < 1 || day > 31) {
		cout << "日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。\n";
		return;
	}

	// Description input
	cout << "输入描述 (最多 " << Expense::MAX_DESCRIPTION_LENGTH << " 字符, 输入 '!cancel' 取消): ";
	getline(cin, description);
	if (description == "!cancel") { cout << "已取消添加开销。\n"; return; }
	if (description.length() > Expense::MAX_DESCRIPTION_LENGTH) {
		cout << "描述过长，已截断为 " << Expense::MAX_DESCRIPTION_LENGTH << " 字符。\n";
		description = description.substr(0, Expense::MAX_DESCRIPTION_LENGTH);
	}

	// Amount input
	cout << "输入金额 (-1 取消): ";
	while (true) {
		getline(cin, line_input);
		if (line_input == "-1") { cout << "已取消添加开销。\n"; return; }
		stringstream ss_amount(line_input);
		if (ss_amount >> amount && amount >= 0 && ss_amount.eof()) {
			break;
		}
		cout << "金额无效或为负，请重新输入 (-1 取消): ";
	}

	// Category input
	cout << "输入类别 (如 餐饮, 交通, 娱乐; 最多 " << Expense::MAX_CATEGORY_LENGTH << " 字符, 输入 '!cancel' 取消): ";
	getline(cin, category);
	if (category == "!cancel") { cout << "已取消添加开销。\n"; return; }
	if (category.length() > Expense::MAX_CATEGORY_LENGTH) {
		cout << "类别名称过长，已截断为 " << Expense::MAX_CATEGORY_LENGTH << " 字符。\n";
		category = category.substr(0, Expense::MAX_CATEGORY_LENGTH);
	}
	if (category.empty()) {
		category = "未分类";
	}

	allExpenses[expenseCount].setData(year, month, day, description, amount, category);
	expenseCount++;
	cout << "开销已添加。\n";
}

void ExpenseTracker::displayAllExpenses() {
	if (__builtin_expect(expenseCount == 0, 0)) {
		cout << "没有开销记录。\n";
		return;
	}
	cout << "\n--- 所有开销记录 ---\n";
	print_table_header();
	print_separator(12 + 30 + 20 + 10);

	for (int i = 0; i < expenseCount; ++i) {
		print_expense_record(allExpenses[i]);
	}
	print_separator(12 + 30 + 20 + 10);
}

void ExpenseTracker::displayMonthlySummary() {
	int year, month;
	cout << "\n--- 月度开销统计 ---\n";

	cout << "输入要统计的年份 (YYYY) (-1 取消): ";
	while (true) {
		cin >> year;
		if (cin.fail()) {
			cout << "年份输入无效，请重新输入 (-1 取消): ";
			cin.clear();
			clearInputBuffer();
		} else if (year == -1) {
			clearInputBuffer();
			cout << "已取消月度统计。\n";
			return;
		} else {
			clearInputBuffer();
			break;
		}
	}

	cout << "输入要统计的月份 (MM) (-1 取消): ";
	while (true) {
		cin >> month;
		if (cin.fail()) {
			cout << "月份输入无效 (1-12)，请重新输入 (-1 取消): ";
			cin.clear();
			clearInputBuffer();
		} else if (month == -1) {
			clearInputBuffer();
			cout << "已取消月度统计。\n";
			return;
		} else if (month < 1 || month > 12) {
			cout << "月份输入无效 (1-12)，请重新输入 (-1 取消): ";
			clearInputBuffer();
		} else {
			clearInputBuffer();
			break;
		}
	}

	cout << "\n--- " << year << "年" << setfill('0') << setw(2) << month << setfill(' ') << "月 开销统计 ---\n";

	double totalMonthAmount = 0;
	bool foundRecords = false;

	CategorySum categorySums[MAX_UNIQUE_CATEGORIES_PER_MONTH];
	int uniqueCategoriesCount = 0;
	double maxCategoryTotal = 0.0;

	print_table_header();
	print_separator(12 + 30 + 20 + 10);

	for (int i = 0; i < expenseCount; ++i) {
		if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month) {
			foundRecords = true;
			print_expense_record(allExpenses[i]);
			totalMonthAmount += allExpenses[i].getAmount();

			bool categoryExists = false;
			const char* cat = allExpenses[i].getCategory();
			for (int j = 0; j < uniqueCategoriesCount; ++j) {
				if (strcmp(categorySums[j].name, cat) == 0) {
					categorySums[j].total += allExpenses[i].getAmount();
					categoryExists = true;
					if (categorySums[j].total > maxCategoryTotal) maxCategoryTotal = categorySums[j].total;
					break;
				}
			}
			if (!categoryExists && uniqueCategoriesCount < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
				size_t clen = strlen(cat);
				if (clen > 51) clen = 51;
				memcpy(categorySums[uniqueCategoriesCount].name, cat, clen);
				categorySums[uniqueCategoriesCount].name[clen] = '\0';
				categorySums[uniqueCategoriesCount].total = allExpenses[i].getAmount();
				if (categorySums[uniqueCategoriesCount].total > maxCategoryTotal) maxCategoryTotal = categorySums[uniqueCategoriesCount].total;
				uniqueCategoriesCount++;
			}
		}
	}

	if (!foundRecords) {
		cout << "该月份没有开销记录。\n";
	} else {
		print_separator(12 + 30 + 20 + 10);
		cout << left << setw(12 + 30 + 20) << "本月总计:"
			 << right << fixed << setprecision(2) << setw(10) << totalMonthAmount << "\n\n";

		if (uniqueCategoriesCount > 0) {
			cout << "按类别汇总:\n";
			cout << left << setw(20) << "类别" << right << setw(10) << "总金额\n";
			print_separator(30);
			for (int i = 0; i < uniqueCategoriesCount; ++i) {
				cout << left << setw(20) << categorySums[i].name
					 << right << fixed << setprecision(2) << setw(10) << categorySums[i].total << "\n";
			}
			print_separator(30);
		}
	}
}

void ExpenseTracker::listExpensesByPeriod() {
	int choice;
	do {
		cout << "\n--- 按期间列出开销 --- \n";
		cout << "1. 按年份列出\n";
		cout << "2. 按月份列出\n";
		cout << "3. 按日期列出\n";
		cout << "0. 返回主菜单\n";
		cout << "--------------------\n";
		cout << "请输入选项: ";

		cin >> choice;
		if (__builtin_expect(cin.fail(), 0)) {
			cin.clear();
			clearInputBuffer();
			choice = -1;
		} else {
			clearInputBuffer();
		}

		switch (choice) {
			case 1: {
				cout << "\n--- 按年份列出开销 ---\n";
				cout << "输入年份 (YYYY) (输入 0 返回): ";
				int year;
				while (!(cin >> year)) {
					cout << "年份输入无效，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (year == 0) break;

				bool found = false;
				print_table_header();
				print_separator(12 + 30 + 20 + 10);
				for (int i = 0; i < expenseCount; ++i) {
					if (allExpenses[i].getYear() == year) {
						print_expense_record(allExpenses[i]);
						found = true;
					}
				}
				if (!found) {
					cout << "在 " << year << " 年没有找到开销记录。\n";
				}
				print_separator(12 + 30 + 20 + 10);
				break;
			}

			case 2: {
				cout << "\n--- 按月份列出开销 ---\n";
				cout << "输入年份 (YYYY) (输入 0 返回): ";
				int year;
				while (!(cin >> year)) {
					cout << "年份输入无效，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (year == 0) break;

				cout << "输入月份 (MM) (输入 0 返回): ";
				int month;
				while (!(cin >> month) || (month != 0 && (month < 1 || month > 12))) {
					cout << "月份输入无效 (1-12)，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (month == 0) break;

				bool found = false;
				// Original code: header missing 描述 and 类别 columns
				cout << left << setw(12) << "日期" << setw(10) << "金额\n";
				print_separator(12 + 30 + 20 + 10);
				for (int i = 0; i < expenseCount; ++i) {
					if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month) {
						// Original code: only outputs left alignment + newline
						cout << left << "\n";
						found = true;
					}
				}
				if (!found) {
					cout << "在 " << year << " 年 " << month << " 月没有找到开销记录。\n";
				}
				print_separator(12 + 30 + 20 + 10);
				break;
			}

			case 3: {
				cout << "\n--- 按日期列出开销 ---\n";
				cout << "输入年份 (YYYY) (输入 0 返回): ";
				int year;
				while (!(cin >> year)) {
					cout << "年份输入无效，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (year == 0) break;

				cout << "输入月份 (MM) (输入 0 返回): ";
				int month;
				while (!(cin >> month) || (month != 0 && (month < 1 || month > 12))) {
					cout << "月份输入无效 (1-12)，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (month == 0) break;

				cout << "输入日期 (DD) (输入 0 返回): ";
				int day;
				while (!(cin >> day) || (day != 0 && (day < 1 || day > 31))) {
					cout << "日期输入无效 (1-31)，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (day == 0) break;

				bool found = false;
				// Original code: header missing 描述 and 类别 columns
				cout << left << setw(12) << "日期" << setw(10) << "金额\n";
				print_separator(12 + 30 + 20 + 10);
				for (int i = 0; i < expenseCount; ++i) {
					if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month && allExpenses[i].getDay() == day) {
						// Original code: only outputs left alignment + newline
						cout << left << "\n";
						found = true;
					}
				}
				if (!found) {
					cout << "在 " << year << " 年 " << month << " 月 " << day << " 日没有找到开销记录。\n";
				}
				print_separator(12 + 30 + 20 + 10);
				break;
			}

			case 0:
				cout << "返回主菜单...\n";
				break;
			default:
				cout << "无效选项，请重试。\n";
		}
	} while (choice != 0);
}

void ExpenseTracker::saveExpenses() {
	// Use FILE* for faster I/O
	FILE* fp = fopen(DATA_FILE, "w");
	if (__builtin_expect(!fp, 0)) {
		cerr << "错误：无法打开文件 " << DATA_FILE << " 进行写入！\n";
		return;
	}

	// Write count
	fprintf(fp, "%d\n", expenseCount);

	// Write each record
	char line_buf[512];
	for (int i = 0; i < expenseCount; ++i) {
		const Expense& e = allExpenses[i];
		int len = snprintf(line_buf, sizeof(line_buf), "%d,%d,%d,%s,%g,%s\n",
			e.getYear(), e.getMonth(), e.getDay(),
			e.getDescription(), e.getAmount(), e.getCategory());
		fwrite(line_buf, 1, len, fp);
	}

	fclose(fp);
}

bool ExpenseTracker::loadExpenses() {
	// Use FILE* for faster I/O
	FILE* fp = fopen(DATA_FILE, "r");
	if (__builtin_expect(!fp, 0)) {
		return false;
	}

	int countFromFile;
	if (fscanf(fp, "%d\n", &countFromFile) != 1 || countFromFile < 0 || countFromFile > MAX_EXPENSES) {
		expenseCount = 0;
		fclose(fp);
		return false;
	}

	char line_buf[1024];
	int loadedCount = 0;

	for (int i = 0; i < countFromFile; ++i) {
		if (!fgets(line_buf, sizeof(line_buf), fp)) {
			break;
		}

		// Remove trailing newline
		size_t line_len = strlen(line_buf);
		if (line_len > 0 && line_buf[line_len-1] == '\n') {
			line_buf[--line_len] = '\0';
		}

		// Parse fields manually for speed
		// Format: year,month,day,description,amount,category
		char* p = line_buf;
		char* end;

		// Parse year
		long year_val = strtol(p, &end, 10);
		if (*end != ',') continue;
		p = end + 1;

		// Parse month
		long month_val = strtol(p, &end, 10);
		if (*end != ',') continue;
		p = end + 1;

		// Parse day
		long day_val = strtol(p, &end, 10);
		if (*end != ',') continue;
		p = end + 1;

		// Parse description (up to next comma)
		char* desc_start = p;
		char* comma = strchr(p, ',');
		if (!comma) continue;
		size_t desc_len = comma - desc_start;
		p = comma + 1;

		// Parse amount (up to next comma)
		double amt = strtod(p, &end);
		if (*end != ',') continue;
		p = end + 1;

		// Parse category (rest of line)
		char* cat_start = p;
		size_t cat_len = line_len - (cat_start - line_buf);

		if (loadedCount < MAX_EXPENSES) {
			allExpenses[loadedCount].setDataRaw(
				(int)year_val, (int)month_val, (int)day_val,
				desc_start, desc_len,
				amt,
				cat_start, cat_len
			);
			loadedCount++;
		} else {
			break;
		}
	}

	expenseCount = loadedCount;
	fclose(fp);
	return true;
}

void ExpenseTracker::readLastSettlement(int& lastYear, int& lastMonth) {
	lastYear = 0;
	lastMonth = 0;
	FILE* fp = fopen(SETTLEMENT_FILE, "r");
	if (fp) {
		fscanf(fp, "%d %d", &lastYear, &lastMonth);
		fclose(fp);
	}
}

void ExpenseTracker::writeLastSettlement(int year, int month) {
	FILE* fp = fopen(SETTLEMENT_FILE, "w");
	if (fp) {
		fprintf(fp, "%d %d\n", year, month);
		fclose(fp);
	} else {
		cerr << "错误：无法写入结算状态文件 " << SETTLEMENT_FILE << "\n";
	}
}

void ExpenseTracker::generateMonthlyReportForSettlement(int year, int month) {
	cout << "\n--- " << year << "年" << setfill('0') << setw(2) << month << setfill(' ') << "月 开销报告 (自动结算) ---\n";

	double totalMonthAmount = 0;
	bool foundRecords = false;

	CategorySum categorySums[MAX_UNIQUE_CATEGORIES_PER_MONTH];
	int uniqueCategoriesCount = 0;
	double maxCategoryTotal = 0.0;

	cout << "明细:\n";
	print_table_header();
	print_separator(12 + 30 + 20 + 10);

	for (int i = 0; i < expenseCount; ++i) {
		if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month) {
			foundRecords = true;
			print_expense_record(allExpenses[i]);
			totalMonthAmount += allExpenses[i].getAmount();

			bool categoryExists = false;
			const char* cat = allExpenses[i].getCategory();
			for (int j = 0; j < uniqueCategoriesCount; ++j) {
				if (strcmp(categorySums[j].name, cat) == 0) {
					categorySums[j].total += allExpenses[i].getAmount();
					categoryExists = true;
					if (categorySums[j].total > maxCategoryTotal) maxCategoryTotal = categorySums[j].total;
					break;
				}
			}
			if (!categoryExists && uniqueCategoriesCount < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
				size_t clen = strlen(cat);
				if (clen > 51) clen = 51;
				memcpy(categorySums[uniqueCategoriesCount].name, cat, clen);
				categorySums[uniqueCategoriesCount].name[clen] = '\0';
				categorySums[uniqueCategoriesCount].total = allExpenses[i].getAmount();
				if (categorySums[uniqueCategoriesCount].total > maxCategoryTotal) maxCategoryTotal = categorySums[uniqueCategoriesCount].total;
				uniqueCategoriesCount++;
			}
		}
	}

	if (!foundRecords) {
		cout << "该月份没有开销记录。\n";
		return;
	}

	print_separator(12 + 30 + 20 + 10);
	cout << left << setw(12 + 30 + 20) << "本月总计:"
		 << right << fixed << setprecision(2) << setw(10) << totalMonthAmount << "\n\n";

	if (uniqueCategoriesCount > 0) {
		cout << "按类别汇总:\n";
		cout << left << setw(20) << "类别" << right << setw(10) << "总金额\n";
		print_separator(30);
		for (int i = 0; i < uniqueCategoriesCount; ++i) {
			cout << left << setw(20) << categorySums[i].name
				 << right << fixed << setprecision(2) << setw(10) << categorySums[i].total << "\n";
		}
		print_separator(30);
	}

	cout << "--- 报告生成完毕 ---\n";
}

void ExpenseTracker::performAutomaticSettlement() {
	int lastSettledYear, lastSettledMonth;
	readLastSettlement(lastSettledYear, lastSettledMonth);

	time_t now = time(0);
	tm *ltm = localtime(&now);
	int currentYear = 1900 + ltm->tm_year;
	int currentMonth = 1 + ltm->tm_mon;

	if (lastSettledYear == 0) {
		lastSettledYear = currentYear;
		lastSettledMonth = currentMonth;
		if (lastSettledMonth == 1) {
			lastSettledMonth = 12;
			lastSettledYear--;
		} else {
			lastSettledMonth--;
		}
		writeLastSettlement(lastSettledYear, lastSettledMonth);
		cout << "首次运行或无结算记录，已设置基准结算点为: "
			 << lastSettledYear << "年" << setfill('0') << setw(2) << lastSettledMonth << setfill(' ') << "月。\n";
		return;
	}

	int yearToSettle = lastSettledYear;
	int monthToSettle = lastSettledMonth;

	while (true) {
		monthToSettle++;
		if (monthToSettle > 12) {
			monthToSettle = 1;
			yearToSettle++;
		}

		if (yearToSettle > currentYear || (yearToSettle == currentYear && monthToSettle >= currentMonth)) {
			break;
		}

		cout << "\n>>> 开始自动结算: " << yearToSettle << "年" << setfill('0') << setw(2) << monthToSettle << setfill(' ') << "月 <<\n";
		generateMonthlyReportForSettlement(yearToSettle, monthToSettle);
		writeLastSettlement(yearToSettle, monthToSettle);
		cout << ">>> 自动结算完成: " << yearToSettle << "年" << setfill('0') << setw(2) << monthToSettle << setfill(' ') << "月 <<\n";
	}
}

void ExpenseTracker::deleteExpense() {
	if (__builtin_expect(expenseCount == 0, 0)) {
		cout << "没有开销记录可供删除。\n";
		return;
	}

	cout << "\n--- 删除开销记录 ---\n";
	cout << "以下是所有开销记录:\n";
	cout << left
		 << setw(5) << "序号"
		 << setw(12) << "日期"
		 << setw(30) << "描述"
		 << setw(20) << "类别"
		 << right << setw(10) << "金额\n";
	print_separator(5 + 12 + 30 + 20 + 10);

	for (int i = 0; i < expenseCount; ++i) {
		cout << left
			 << setw(5) << i + 1
			 << setw(4) << allExpenses[i].getYear() << "-"
			 << setfill('0') << setw(2) << allExpenses[i].getMonth() << "-"
			 << setw(2) << allExpenses[i].getDay() << setfill(' ') << "  "
			 << setw(30) << allExpenses[i].getDescription()
			 << setw(20) << allExpenses[i].getCategory()
			 << right << fixed << setprecision(2) << setw(10) << allExpenses[i].getAmount() << "\n";
	}
	print_separator(5 + 12 + 30 + 20 + 10);

	int recordNumberToDelete;
	cout << "请输入要删除的记录序号 (0 取消删除): ";
	while (!(cin >> recordNumberToDelete) || recordNumberToDelete < 0 || recordNumberToDelete > expenseCount) {
		cout << "输入无效。请输入 1 到 " << expenseCount << " 之间的数字，或 0 取消: ";
		cin.clear();
		clearInputBuffer();
	}
	clearInputBuffer();

	if (recordNumberToDelete == 0) {
		cout << "取消删除操作。\n";
		return;
	}

	int indexToDelete = recordNumberToDelete - 1;

	cout << "\n即将删除以下记录:\n";
	cout << left
		 << setw(12) << "日期"
		 << setw(30) << "描述"
		 << setw(20) << "类别"
		 << right << setw(10) << "金额\n";
	print_separator(12 + 30 + 20 + 10);
	print_expense_record(allExpenses[indexToDelete]);
	print_separator(12 + 30 + 20 + 10);

	char confirm;
	cout << "确认删除吗？ (y/n): ";
	cin >> confirm;
	clearInputBuffer();

	if (confirm == 'y' || confirm == 'Y') {
		cout << "\n警告：此操作无法撤销！\n";
		cout << "最后一次确认，真的要删除这条记录吗？ (y/n): ";
		char final_confirm;
		cin >> final_confirm;
		clearInputBuffer();

		if (final_confirm == 'y' || final_confirm == 'Y') {
			cout << "\n正在删除记录...\n";
			// Use memmove for faster bulk copy
			if (indexToDelete < expenseCount - 1) {
				memmove(&allExpenses[indexToDelete], &allExpenses[indexToDelete + 1],
					sizeof(Expense) * (expenseCount - 1 - indexToDelete));
			}
			expenseCount--;
			cout << "记录已删除。\n";
			saveExpenses();
			cout << "数据已自动保存。\n";
		} else {
			cout << "已取消删除操作（二次确认未通过）。\n";
		}
	} else {
		cout << "取消删除操作。\n";
	}
}

int main() {
	// I/O optimization
	ios_base::sync_with_stdio(false);
	cin.tie(nullptr);
	setvbuf(stdout, stdout_buf, _IOFBF, sizeof(stdout_buf));

	ExpenseTracker tracker;
	tracker.run();
	return 0;
}

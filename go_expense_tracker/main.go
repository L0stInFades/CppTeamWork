package main

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

const (
	MaxExpenses                = 1000
	MaxUniqueCategoriesPerMonth = 20
	DataFile                   = "expenses.dat"
	SettlementFile             = "settlement_status.txt"
	MaxDescriptionLength       = 100
	MaxCategoryLength          = 50
)

type Expense struct {
	Year        int
	Month       int
	Day         int
	Description string
	Amount      float64
	Category    string
}

type CategorySum struct {
	Name  string
	Total float64
}

type ExpenseTracker struct {
	allExpenses  [MaxExpenses]Expense
	expenseCount int
	scanner      *bufio.Scanner
}

var stdin *bufio.Scanner

func init() {
	stdin = bufio.NewScanner(os.Stdin)
	stdin.Buffer(make([]byte, 1024*1024), 1024*1024)
}

func readLine() string {
	if stdin.Scan() {
		return stdin.Text()
	}
	return ""
}

func readInt() (int, bool) {
	line := readLine()
	line = strings.TrimSpace(line)
	n, err := strconv.Atoi(line)
	if err != nil {
		return 0, false
	}
	return n, true
}

func printExpenseHeader() {
	fmt.Printf("%-12s%-30s%-20s%10s\n", "日期", "描述", "类别", "金额")
	fmt.Println(strings.Repeat("-", 72))
}

func printExpenseHeaderWithIndex() {
	fmt.Printf("%-5s%-12s%-30s%-20s%10s\n", "序号", "日期", "描述", "类别", "金额")
	fmt.Println(strings.Repeat("-", 77))
}

func printExpenseRow(exp *Expense) {
	fmt.Printf("%-4d-%02d-%02d  %-30s%-20s%10.2f\n",
		exp.Year, exp.Month, exp.Day, exp.Description, exp.Category, exp.Amount)
}

func printExpenseRowWithIndex(index int, exp *Expense) {
	fmt.Printf("%-5d%-4d-%02d-%02d  %-30s%-20s%10.2f\n",
		index, exp.Year, exp.Month, exp.Day, exp.Description, exp.Category, exp.Amount)
}

func NewExpenseTracker() *ExpenseTracker {
	tracker := &ExpenseTracker{}
	if tracker.loadExpenses() {
		fmt.Printf("成功加载 %d 条历史记录。\n", tracker.expenseCount)
	} else {
		fmt.Println("未找到历史数据文件或加载失败，开始新的记录。")
	}
	tracker.performAutomaticSettlement()
	return tracker
}

func (t *ExpenseTracker) run() {
	for {
		fmt.Println("\n大学生开销追踪器")
		fmt.Println("--------------------")
		fmt.Println("1. 添加开销记录")
		fmt.Println("2. 查看所有开销")
		fmt.Println("3. 查看月度统计")
		fmt.Println("4. 按期间列出开销")
		fmt.Println("5. 删除开销记录")
		fmt.Println("6. 保存并退出")
		fmt.Println("--------------------")
		fmt.Print("请输入选项: ")

		choice, ok := readInt()
		if !ok {
			choice = 0
		}

		switch choice {
		case 1:
			t.addExpense()
		case 2:
			t.displayAllExpenses()
		case 3:
			t.displayMonthlySummary()
		case 4:
			t.listExpensesByPeriod()
		case 5:
			t.deleteExpense()
		case 6:
			t.saveExpenses()
			fmt.Println("数据已保存。正在退出...")
			return
		default:
			fmt.Println("无效选项，请重试。")
		}
	}
}

func (t *ExpenseTracker) addExpense() {
	if t.expenseCount >= MaxExpenses {
		fmt.Println("错误：开销记录已满！无法添加更多记录。")
		return
	}

	fmt.Println("\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---")

	now := time.Now()
	currentYear := now.Year()
	currentMonth := int(now.Month())
	currentDay := now.Day()

	// Get year
	fmt.Printf("输入年份 (YYYY) [默认: %d, -1 取消]: ", currentYear)
	lineInput := readLine()
	if lineInput == "-1" {
		fmt.Println("已取消添加开销。")
		return
	}
	year := currentYear
	if lineInput != "" {
		trimmed := strings.TrimSpace(lineInput)
		if n, err := strconv.Atoi(trimmed); err == nil {
			year = n
		} else {
			fmt.Printf("年份输入无效或包含非数字字符，将使用默认年份: %d。\n", currentYear)
		}
	}

	// Get month
	fmt.Printf("输入月份 (MM) [默认: %d, -1 取消]: ", currentMonth)
	lineInput = readLine()
	if lineInput == "-1" {
		fmt.Println("已取消添加开销。")
		return
	}
	month := currentMonth
	if lineInput != "" {
		trimmed := strings.TrimSpace(lineInput)
		if n, err := strconv.Atoi(trimmed); err == nil && n >= 1 && n <= 12 {
			month = n
		} else {
			fmt.Printf("月份输入无效或范围不正确 (1-12)，将使用默认月份: %d。\n", currentMonth)
		}
	}

	// Get day
	fmt.Printf("输入日期 (DD) [默认: %d, -1 取消]: ", currentDay)
	lineInput = readLine()
	if lineInput == "-1" {
		fmt.Println("已取消添加开销。")
		return
	}
	day := currentDay
	if lineInput != "" {
		trimmed := strings.TrimSpace(lineInput)
		if n, err := strconv.Atoi(trimmed); err == nil && n >= 1 && n <= 31 {
			day = n
		} else {
			fmt.Printf("日期输入无效或范围不正确 (1-31)，将使用默认日期: %d。\n", currentDay)
		}
	}

	// Basic date validation
	if month < 1 || month > 12 || day < 1 || day > 31 {
		fmt.Println("日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。")
		return
	}

	// Get description
	fmt.Printf("输入描述 (最多 %d 字符, 输入 '!cancel' 取消): ", MaxDescriptionLength)
	description := readLine()
	if description == "!cancel" {
		fmt.Println("已取消添加开销。")
		return
	}
	if len(description) > MaxDescriptionLength {
		fmt.Printf("描述过长，已截断为 %d 字符。\n", MaxDescriptionLength)
		description = truncateString(description, MaxDescriptionLength)
	}

	// Get amount
	fmt.Print("输入金额 (-1 取消): ")
	var amount float64
	for {
		lineInput = readLine()
		if lineInput == "-1" {
			fmt.Println("已取消添加开销。")
			return
		}
		trimmed := strings.TrimSpace(lineInput)
		if a, err := strconv.ParseFloat(trimmed, 64); err == nil && a >= 0 {
			amount = a
			break
		}
		fmt.Print("金额无效或为负，请重新输入 (-1 取消): ")
	}

	// Get category
	fmt.Printf("输入类别 (如 餐饮, 交通, 娱乐; 最多 %d 字符, 输入 '!cancel' 取消): ", MaxCategoryLength)
	category := readLine()
	if category == "!cancel" {
		fmt.Println("已取消添加开销。")
		return
	}
	if len(category) > MaxCategoryLength {
		fmt.Printf("类别名称过长，已截断为 %d 字符。\n", MaxCategoryLength)
		category = truncateString(category, MaxCategoryLength)
	}
	if category == "" {
		category = "未分类"
	}

	t.allExpenses[t.expenseCount] = Expense{
		Year:        year,
		Month:       month,
		Day:         day,
		Description: description,
		Amount:      amount,
		Category:    category,
	}
	t.expenseCount++
	fmt.Println("开销已添加。")
}

func (t *ExpenseTracker) displayAllExpenses() {
	if t.expenseCount == 0 {
		fmt.Println("没有开销记录。")
		return
	}
	fmt.Println("\n--- 所有开销记录 ---")
	printExpenseHeader()
	for i := 0; i < t.expenseCount; i++ {
		printExpenseRow(&t.allExpenses[i])
	}
	fmt.Println(strings.Repeat("-", 72))
}

func (t *ExpenseTracker) displayMonthlySummary() {
	fmt.Println("\n--- 月度开销统计 ---")

	// Get year
	fmt.Print("输入要统计的年份 (YYYY) (-1 取消): ")
	var year int
	for {
		n, ok := readInt()
		if ok {
			if n == -1 {
				fmt.Println("已取消月度统计。")
				return
			}
			year = n
			break
		}
		fmt.Print("年份输入无效，请重新输入 (-1 取消): ")
	}

	// Get month
	fmt.Print("输入要统计的月份 (MM) (-1 取消): ")
	var month int
	for {
		n, ok := readInt()
		if ok {
			if n == -1 {
				fmt.Println("已取消月度统计。")
				return
			}
			if n >= 1 && n <= 12 {
				month = n
				break
			}
			fmt.Print("月份输入无效 (1-12)，请重新输入 (-1 取消): ")
		} else {
			fmt.Print("月份输入无效 (1-12)，请重新输入 (-1 取消): ")
		}
	}

	fmt.Printf("\n--- %d年%02d月 开销统计 ---\n", year, month)

	totalMonthAmount := 0.0
	foundRecords := false
	categorySums := make([]CategorySum, 0, MaxUniqueCategoriesPerMonth)
	maxCategoryTotal := 0.0
	_ = maxCategoryTotal

	printExpenseHeader()

	for i := 0; i < t.expenseCount; i++ {
		exp := &t.allExpenses[i]
		if exp.Year == year && exp.Month == month {
			foundRecords = true
			printExpenseRow(exp)
			totalMonthAmount += exp.Amount

			// Category aggregation
			categoryExists := false
			for j := range categorySums {
				if categorySums[j].Name == exp.Category {
					categorySums[j].Total += exp.Amount
					categoryExists = true
					if categorySums[j].Total > maxCategoryTotal {
						maxCategoryTotal = categorySums[j].Total
					}
					break
				}
			}
			if !categoryExists && len(categorySums) < MaxUniqueCategoriesPerMonth {
				cs := CategorySum{Name: exp.Category, Total: exp.Amount}
				if cs.Total > maxCategoryTotal {
					maxCategoryTotal = cs.Total
				}
				categorySums = append(categorySums, cs)
			}
		}
	}

	if !foundRecords {
		fmt.Println("该月份没有开销记录。")
	} else {
		fmt.Println(strings.Repeat("-", 72))
		fmt.Printf("%-62s%10.2f\n", "本月总计:", totalMonthAmount)
		fmt.Println()

		if len(categorySums) > 0 {
			fmt.Println("按类别汇总:")
			fmt.Printf("%-20s%10s\n", "类别", "总金额")
			fmt.Println(strings.Repeat("-", 30))
			for _, cs := range categorySums {
				fmt.Printf("%-20s%10.2f\n", cs.Name, cs.Total)
			}
			fmt.Println(strings.Repeat("-", 30))
		}
	}
}

func (t *ExpenseTracker) listExpensesByPeriod() {
	for {
		fmt.Println("\n--- 按期间列出开销 --- ")
		fmt.Println("1. 按年份列出")
		fmt.Println("2. 按月份列出")
		fmt.Println("3. 按日期列出")
		fmt.Println("0. 返回主菜单")
		fmt.Println("--------------------")
		fmt.Print("请输入选项: ")

		choice, ok := readInt()
		if !ok {
			choice = -1
		}

		switch choice {
		case 1:
			fmt.Println("\n--- 按年份列出开销 ---")
			fmt.Print("输入年份 (YYYY) (输入 0 返回): ")
			var year int
			for {
				n, ok := readInt()
				if ok {
					year = n
					break
				}
				fmt.Print("年份输入无效，请重新输入 (输入 0 返回): ")
			}
			if year == 0 {
				continue
			}

			found := false
			printExpenseHeader()
			for i := 0; i < t.expenseCount; i++ {
				if t.allExpenses[i].Year == year {
					printExpenseRow(&t.allExpenses[i])
					found = true
				}
			}
			if !found {
				fmt.Printf("在 %d 年没有找到开销记录。\n", year)
			}
			fmt.Println(strings.Repeat("-", 72))

		case 2:
			fmt.Println("\n--- 按月份列出开销 ---")
			fmt.Print("输入年份 (YYYY) (输入 0 返回): ")
			var year int
			for {
				n, ok := readInt()
				if ok {
					year = n
					break
				}
				fmt.Print("年份输入无效，请重新输入 (输入 0 返回): ")
			}
			if year == 0 {
				continue
			}

			fmt.Print("输入月份 (MM) (输入 0 返回): ")
			var month int
			for {
				n, ok := readInt()
				if ok && (n == 0 || (n >= 1 && n <= 12)) {
					month = n
					break
				}
				fmt.Print("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ")
			}
			if month == 0 {
				continue
			}

			found := false
			printExpenseHeader()
			for i := 0; i < t.expenseCount; i++ {
				exp := &t.allExpenses[i]
				if exp.Year == year && exp.Month == month {
					printExpenseRow(exp)
					found = true
				}
			}
			if !found {
				fmt.Printf("在 %d 年 %d 月没有找到开销记录。\n", year, month)
			}
			fmt.Println(strings.Repeat("-", 72))

		case 3:
			fmt.Println("\n--- 按日期列出开销 ---")
			fmt.Print("输入年份 (YYYY) (输入 0 返回): ")
			var year int
			for {
				n, ok := readInt()
				if ok {
					year = n
					break
				}
				fmt.Print("年份输入无效，请重新输入 (输入 0 返回): ")
			}
			if year == 0 {
				continue
			}

			fmt.Print("输入月份 (MM) (输入 0 返回): ")
			var month int
			for {
				n, ok := readInt()
				if ok && (n == 0 || (n >= 1 && n <= 12)) {
					month = n
					break
				}
				fmt.Print("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ")
			}
			if month == 0 {
				continue
			}

			fmt.Print("输入日期 (DD) (输入 0 返回): ")
			var day int
			for {
				n, ok := readInt()
				if ok && (n == 0 || (n >= 1 && n <= 31)) {
					day = n
					break
				}
				fmt.Print("日期输入无效 (1-31)，请重新输入 (输入 0 返回): ")
			}
			if day == 0 {
				continue
			}

			found := false
			printExpenseHeader()
			for i := 0; i < t.expenseCount; i++ {
				exp := &t.allExpenses[i]
				if exp.Year == year && exp.Month == month && exp.Day == day {
					printExpenseRow(exp)
					found = true
				}
			}
			if !found {
				fmt.Printf("在 %d 年 %d 月 %d 日没有找到开销记录。\n", year, month, day)
			}
			fmt.Println(strings.Repeat("-", 72))

		case 0:
			fmt.Println("返回主菜单...")
			return

		default:
			fmt.Println("无效选项，请重试。")
		}
	}
}

func (t *ExpenseTracker) saveExpenses() {
	var sb strings.Builder
	sb.WriteString(fmt.Sprintf("%d\n", t.expenseCount))
	for i := 0; i < t.expenseCount; i++ {
		exp := &t.allExpenses[i]
		sb.WriteString(fmt.Sprintf("%d,%d,%d,%s,%g,%s\n",
			exp.Year, exp.Month, exp.Day, exp.Description, exp.Amount, exp.Category))
	}
	err := os.WriteFile(DataFile, []byte(sb.String()), 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "错误：无法打开文件 %s 进行写入！\n", DataFile)
	}
}

func (t *ExpenseTracker) loadExpenses() bool {
	data, err := os.ReadFile(DataFile)
	if err != nil {
		return false
	}

	content := string(data)
	lines := strings.Split(strings.TrimRight(content, "\n"), "\n")
	if len(lines) == 0 {
		t.expenseCount = 0
		return false
	}

	countFromFile, err := strconv.Atoi(strings.TrimSpace(lines[0]))
	if err != nil || countFromFile < 0 || countFromFile > MaxExpenses {
		t.expenseCount = 0
		return false
	}

	loadedCount := 0
	for i := 1; i < len(lines) && i-1 < countFromFile; i++ {
		if loadedCount >= MaxExpenses {
			break
		}

		line := lines[i]
		// Split into at most 6 parts
		parts := splitN(line, ',', 6)

		// Parse year
		if len(parts) < 1 {
			fmt.Fprintf(os.Stderr, "警告：记录 %d 数据不完整 (年份)。\n", i)
			continue
		}
		year, err := strconv.Atoi(parts[0])
		if err != nil {
			fmt.Fprintf(os.Stderr, "警告：无效年份格式 '%s' 在记录 %d。跳过此记录。\n", parts[0], i)
			continue
		}

		// Parse month
		if len(parts) < 2 {
			fmt.Fprintf(os.Stderr, "警告：记录 %d 数据不完整 (月份)。\n", i)
			continue
		}
		month, err := strconv.Atoi(parts[1])
		if err != nil {
			fmt.Fprintf(os.Stderr, "警告：无效月份格式 '%s' 在记录 %d。跳过此记录。\n", parts[1], i)
			continue
		}

		// Parse day
		if len(parts) < 3 {
			fmt.Fprintf(os.Stderr, "警告：记录 %d 数据不完整 (日期)。\n", i)
			continue
		}
		day, err := strconv.Atoi(parts[2])
		if err != nil {
			fmt.Fprintf(os.Stderr, "警告：无效日期格式 '%s' 在记录 %d。跳过此记录。\n", parts[2], i)
			continue
		}

		// Parse description
		if len(parts) < 4 {
			fmt.Fprintf(os.Stderr, "警告：记录 %d 数据不完整 (描述)。\n", i)
			continue
		}
		descriptionStr := parts[3]
		if len(descriptionStr) > MaxDescriptionLength {
			descriptionStr = truncateString(descriptionStr, MaxDescriptionLength)
		}

		// Parse amount
		if len(parts) < 5 {
			fmt.Fprintf(os.Stderr, "警告：记录 %d 数据不完整 (金额)。\n", i)
			continue
		}
		amount, err := strconv.ParseFloat(parts[4], 64)
		if err != nil {
			fmt.Fprintf(os.Stderr, "警告：无效金额格式 '%s' 在记录 %d。跳过此记录。\n", parts[4], i)
			continue
		}

		// Parse category
		categoryStr := ""
		if len(parts) >= 6 {
			categoryStr = parts[5]
			if len(categoryStr) > MaxCategoryLength {
				categoryStr = truncateString(categoryStr, MaxCategoryLength)
			}
		}

		t.allExpenses[loadedCount] = Expense{
			Year:        year,
			Month:       month,
			Day:         day,
			Description: descriptionStr,
			Amount:      amount,
			Category:    categoryStr,
		}
		loadedCount++
	}

	t.expenseCount = loadedCount
	return true
}

func (t *ExpenseTracker) readLastSettlement() (int, int) {
	lastYear := 0
	lastMonth := 0
	data, err := os.ReadFile(SettlementFile)
	if err != nil {
		return lastYear, lastMonth
	}
	parts := strings.Fields(strings.TrimSpace(string(data)))
	if len(parts) >= 1 {
		lastYear, _ = strconv.Atoi(parts[0])
	}
	if len(parts) >= 2 {
		lastMonth, _ = strconv.Atoi(parts[1])
	}
	return lastYear, lastMonth
}

func (t *ExpenseTracker) writeLastSettlement(year, month int) {
	err := os.WriteFile(SettlementFile, []byte(fmt.Sprintf("%d %d\n", year, month)), 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "错误：无法写入结算状态文件 %s\n", SettlementFile)
	}
}

func (t *ExpenseTracker) generateMonthlyReportForSettlement(year, month int) {
	fmt.Printf("\n--- %d年%02d月 开销报告 (自动结算) ---\n", year, month)

	totalMonthAmount := 0.0
	foundRecords := false
	categorySums := make([]CategorySum, 0, MaxUniqueCategoriesPerMonth)
	maxCategoryTotal := 0.0
	_ = maxCategoryTotal

	fmt.Println("明细:")
	printExpenseHeader()

	for i := 0; i < t.expenseCount; i++ {
		exp := &t.allExpenses[i]
		if exp.Year == year && exp.Month == month {
			foundRecords = true
			printExpenseRow(exp)
			totalMonthAmount += exp.Amount

			categoryExists := false
			for j := range categorySums {
				if categorySums[j].Name == exp.Category {
					categorySums[j].Total += exp.Amount
					categoryExists = true
					if categorySums[j].Total > maxCategoryTotal {
						maxCategoryTotal = categorySums[j].Total
					}
					break
				}
			}
			if !categoryExists && len(categorySums) < MaxUniqueCategoriesPerMonth {
				cs := CategorySum{Name: exp.Category, Total: exp.Amount}
				if cs.Total > maxCategoryTotal {
					maxCategoryTotal = cs.Total
				}
				categorySums = append(categorySums, cs)
			}
		}
	}

	if !foundRecords {
		fmt.Println("该月份没有开销记录。")
		return
	}

	fmt.Println(strings.Repeat("-", 72))
	fmt.Printf("%-62s%10.2f\n", "本月总计:", totalMonthAmount)
	fmt.Println()

	if len(categorySums) > 0 {
		fmt.Println("按类别汇总:")
		fmt.Printf("%-20s%10s\n", "类别", "总金额")
		fmt.Println(strings.Repeat("-", 30))
		for _, cs := range categorySums {
			fmt.Printf("%-20s%10.2f\n", cs.Name, cs.Total)
		}
		fmt.Println(strings.Repeat("-", 30))
	}

	fmt.Println("--- 报告生成完毕 ---")
}

func (t *ExpenseTracker) performAutomaticSettlement() {
	lastSettledYear, lastSettledMonth := t.readLastSettlement()
	now := time.Now()
	currentYear := now.Year()
	currentMonth := int(now.Month())

	if lastSettledYear == 0 {
		lastSettledYear = currentYear
		lastSettledMonth = currentMonth
		if lastSettledMonth == 1 {
			lastSettledMonth = 12
			lastSettledYear--
		} else {
			lastSettledMonth--
		}
		t.writeLastSettlement(lastSettledYear, lastSettledMonth)
		fmt.Printf("首次运行或无结算记录，已设置基准结算点为: %d年%02d月。\n",
			lastSettledYear, lastSettledMonth)
		return
	}

	yearToSettle := lastSettledYear
	monthToSettle := lastSettledMonth

	for {
		monthToSettle++
		if monthToSettle > 12 {
			monthToSettle = 1
			yearToSettle++
		}

		if yearToSettle > currentYear || (yearToSettle == currentYear && monthToSettle >= currentMonth) {
			break
		}

		fmt.Printf("\n>>> 开始自动结算: %d年%02d月 <<\n", yearToSettle, monthToSettle)
		t.generateMonthlyReportForSettlement(yearToSettle, monthToSettle)
		t.writeLastSettlement(yearToSettle, monthToSettle)
		fmt.Printf(">>> 自动结算完成: %d年%02d月 <<\n", yearToSettle, monthToSettle)
	}
}

func (t *ExpenseTracker) deleteExpense() {
	if t.expenseCount == 0 {
		fmt.Println("没有开销记录可供删除。")
		return
	}

	fmt.Println("\n--- 删除开销记录 ---")
	fmt.Println("以下是所有开销记录:")
	printExpenseHeaderWithIndex()
	for i := 0; i < t.expenseCount; i++ {
		printExpenseRowWithIndex(i+1, &t.allExpenses[i])
	}
	fmt.Println(strings.Repeat("-", 77))

	// Get record number to delete
	fmt.Print("请输入要删除的记录序号 (0 取消删除): ")
	var recordNumber int
	for {
		n, ok := readInt()
		if ok && n >= 0 && n <= t.expenseCount {
			recordNumber = n
			break
		}
		fmt.Printf("输入无效。请输入 1 到 %d 之间的数字，或 0 取消: ", t.expenseCount)
	}

	if recordNumber == 0 {
		fmt.Println("取消删除操作。")
		return
	}

	indexToDelete := recordNumber - 1

	fmt.Println("\n即将删除以下记录:")
	printExpenseHeader()
	printExpenseRow(&t.allExpenses[indexToDelete])
	fmt.Println(strings.Repeat("-", 72))

	// First confirmation
	fmt.Print("确认删除吗？ (y/n): ")
	confirm := readLine()

	if len(confirm) > 0 && (confirm[0] == 'y' || confirm[0] == 'Y') {
		// Second confirmation
		fmt.Println("\n警告：此操作无法撤销！")
		fmt.Print("最后一次确认，真的要删除这条记录吗？ (y/n): ")
		finalConfirm := readLine()

		if len(finalConfirm) > 0 && (finalConfirm[0] == 'y' || finalConfirm[0] == 'Y') {
			fmt.Println("\n正在删除记录...")
			for i := indexToDelete; i < t.expenseCount-1; i++ {
				t.allExpenses[i] = t.allExpenses[i+1]
			}
			t.expenseCount--
			fmt.Println("记录已删除。")
			t.saveExpenses()
			fmt.Println("数据已自动保存。")
		} else {
			fmt.Println("已取消删除操作（二次确认未通过）。")
		}
	} else {
		fmt.Println("取消删除操作。")
	}
}

// splitN splits a string by a separator into at most n parts
func splitN(s string, sep byte, n int) []string {
	result := make([]string, 0, n)
	for i := 0; i < n-1; i++ {
		idx := strings.IndexByte(s, sep)
		if idx < 0 {
			break
		}
		result = append(result, s[:idx])
		s = s[idx+1:]
	}
	if len(s) > 0 || len(result) > 0 {
		result = append(result, s)
	}
	return result
}

// truncateString truncates a string to maxLen bytes at a valid UTF-8 boundary
func truncateString(s string, maxLen int) string {
	if len(s) <= maxLen {
		return s
	}
	// Find the last valid UTF-8 char boundary at or before maxLen
	end := maxLen
	for end > 0 && !isUTF8Start(s[end]) {
		end--
	}
	return s[:end]
}

func isUTF8Start(b byte) bool {
	// A byte is a UTF-8 start byte if it's ASCII (< 0x80) or a multi-byte start (>= 0xC0)
	return b < 0x80 || b >= 0xC0
}

func main() {
	tracker := NewExpenseTracker()
	tracker.run()
}

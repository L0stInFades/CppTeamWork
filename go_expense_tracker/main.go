package main

import (
	"bufio"
	"os"
	"runtime"
	"runtime/debug"
	"strconv"
	"time"
	"unsafe"
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
	Amount      float64
	Description string
	Category    string
}

type CategorySum struct {
	Name  string
	Total float64
}

type ExpenseTracker struct {
	allExpenses  [MaxExpenses]Expense
	expenseCount int
}

var (
	reader *bufio.Reader
	writer *bufio.Writer
	// Reusable byte buffer for formatting numbers — avoids allocations
	fmtBuf [128]byte
	// Pre-computed repeated separator strings
	dash72 string
	dash77 string
	dash30 string
)

func init() {
	runtime.GOMAXPROCS(1)
	debug.SetGCPercent(-1)
	runtime.MemProfileRate = 0

	reader = bufio.NewReaderSize(os.Stdin, 65536)
	writer = bufio.NewWriterSize(os.Stdout, 65536)

	// Pre-compute separator lines
	b72 := make([]byte, 72)
	for i := range b72 {
		b72[i] = '-'
	}
	dash72 = *(*string)(unsafe.Pointer(&b72))

	b77 := make([]byte, 77)
	for i := range b77 {
		b77[i] = '-'
	}
	dash77 = *(*string)(unsafe.Pointer(&b77))

	b30 := make([]byte, 30)
	for i := range b30 {
		b30[i] = '-'
	}
	dash30 = *(*string)(unsafe.Pointer(&b30))
}

// flushWriter flushes buffered output before reading input
func flushWriter() {
	writer.Flush()
}

// ws writes a string to the buffered writer
func ws(s string) {
	writer.WriteString(s)
}

// wln writes a string followed by newline
func wln(s string) {
	writer.WriteString(s)
	writer.WriteByte('\n')
}

// wnl writes just a newline
func wnl() {
	writer.WriteByte('\n')
}

// writeInt writes an integer to the buffered writer
func writeInt(n int) {
	b := strconv.AppendInt(fmtBuf[:0], int64(n), 10)
	writer.Write(b)
}

// writeIntPadLeft writes an integer left-padded with spaces to width
func writeIntPadLeft(n int, width int) {
	b := strconv.AppendInt(fmtBuf[:0], int64(n), 10)
	for i := len(b); i < width; i++ {
		writer.WriteByte(' ')
	}
	writer.Write(b)
}

// writeIntPadRight writes an integer right-padded with spaces to width
func writeIntPadRight(n int, width int) {
	b := strconv.AppendInt(fmtBuf[:0], int64(n), 10)
	writer.Write(b)
	for i := len(b); i < width; i++ {
		writer.WriteByte(' ')
	}
}

// writeFloat2 writes a float with 2 decimal places right-aligned in width
func writeFloat2(f float64, width int) {
	b := strconv.AppendFloat(fmtBuf[:0], f, 'f', 2, 64)
	for i := len(b); i < width; i++ {
		writer.WriteByte(' ')
	}
	writer.Write(b)
}

// writeStrPad writes a string left-aligned padded to width
func writeStrPad(s string, width int) {
	writer.WriteString(s)
	// Calculate display width — ASCII chars = 1, multi-byte UTF-8 chars counted by runes
	// For CJK characters (3 bytes in UTF-8), display width is 2
	dw := displayWidth(s)
	for i := dw; i < width; i++ {
		writer.WriteByte(' ')
	}
}

// displayWidth estimates terminal display width of a string
// ASCII = 1 column, CJK/wide chars = 2 columns
func displayWidth(s string) int {
	w := 0
	i := 0
	for i < len(s) {
		b := s[i]
		if b < 0x80 {
			w++
			i++
		} else if b < 0xC0 {
			// continuation byte — shouldn't happen at start, skip
			i++
		} else if b < 0xE0 {
			w += 2 // 2-byte sequences are generally wide
			i += 2
		} else if b < 0xF0 {
			w += 2 // 3-byte sequences (CJK) are wide
			i += 3
		} else {
			w += 2 // 4-byte sequences
			i += 4
		}
	}
	return w
}

// writeInt02 writes a zero-padded 2-digit integer
func writeInt02(n int) {
	if n < 10 {
		writer.WriteByte('0')
		writer.WriteByte(byte('0' + n))
	} else {
		b := strconv.AppendInt(fmtBuf[:0], int64(n), 10)
		writer.Write(b)
	}
}

func readLine() string {
	flushWriter()
	line, err := reader.ReadString('\n')
	if err != nil && len(line) == 0 {
		return ""
	}
	// Trim trailing \n and \r\n
	if len(line) > 0 && line[len(line)-1] == '\n' {
		line = line[:len(line)-1]
	}
	if len(line) > 0 && line[len(line)-1] == '\r' {
		line = line[:len(line)-1]
	}
	return line
}

func readInt() (int, bool) {
	line := readLine()
	line = trimSpace(line)
	n, err := strconv.Atoi(line)
	if err != nil {
		return 0, false
	}
	return n, true
}

// trimSpace trims leading and trailing whitespace without allocating if no trim needed
func trimSpace(s string) string {
	start := 0
	for start < len(s) && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n') {
		start++
	}
	end := len(s)
	for end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\r' || s[end-1] == '\n') {
		end--
	}
	return s[start:end]
}

func printExpenseHeader() {
	// "%-12s%-30s%-20s%10s\n"
	writeStrPad("日期", 12)
	writeStrPad("描述", 30)
	writeStrPad("类别", 20)
	writeStrPad("金额", 10)
	wnl()
	wln(dash72)
}

func printExpenseHeaderWithIndex() {
	// "%-5s%-12s%-30s%-20s%10s\n"
	writeStrPad("序号", 5)
	writeStrPad("日期", 12)
	writeStrPad("描述", 30)
	writeStrPad("类别", 20)
	writeStrPad("金额", 10)
	wnl()
	wln(dash77)
}

func printExpenseRow(exp *Expense) {
	// "%-4d-%02d-%02d  %-30s%-20s%10.2f\n"
	writeInt(exp.Year)
	writer.WriteByte('-')
	writeInt02(exp.Month)
	writer.WriteByte('-')
	writeInt02(exp.Day)
	ws("  ")
	writeStrPad(exp.Description, 30)
	writeStrPad(exp.Category, 20)
	writeFloat2(exp.Amount, 10)
	wnl()
}

func printExpenseRowWithIndex(index int, exp *Expense) {
	// "%-5d%-4d-%02d-%02d  %-30s%-20s%10.2f\n"
	writeIntPadRight(index, 5)
	writeInt(exp.Year)
	writer.WriteByte('-')
	writeInt02(exp.Month)
	writer.WriteByte('-')
	writeInt02(exp.Day)
	ws("  ")
	writeStrPad(exp.Description, 30)
	writeStrPad(exp.Category, 20)
	writeFloat2(exp.Amount, 10)
	wnl()
}

func NewExpenseTracker() *ExpenseTracker {
	tracker := &ExpenseTracker{}
	if tracker.loadExpenses() {
		ws("成功加载 ")
		writeInt(tracker.expenseCount)
		wln(" 条历史记录。")
	} else {
		wln("未找到历史数据文件或加载失败，开始新的记录。")
	}
	tracker.performAutomaticSettlement()
	return tracker
}

func (t *ExpenseTracker) run() {
	for {
		wln("\n大学生开销追踪器")
		wln("--------------------")
		wln("1. 添加开销记录")
		wln("2. 查看所有开销")
		wln("3. 查看月度统计")
		wln("4. 按期间列出开销")
		wln("5. 删除开销记录")
		wln("6. 保存并退出")
		wln("--------------------")
		ws("请输入选项: ")

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
			wln("数据已保存。正在退出...")
			flushWriter()
			return
		default:
			wln("无效选项，请重试。")
		}
	}
}

func (t *ExpenseTracker) addExpense() {
	if t.expenseCount >= MaxExpenses {
		wln("错误：开销记录已满！无法添加更多记录。")
		return
	}

	wln("\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---")

	now := time.Now()
	currentYear := now.Year()
	currentMonth := int(now.Month())
	currentDay := now.Day()

	// Get year
	ws("输入年份 (YYYY) [默认: ")
	writeInt(currentYear)
	ws(", -1 取消]: ")
	lineInput := readLine()
	if lineInput == "-1" {
		wln("已取消添加开销。")
		return
	}
	year := currentYear
	if lineInput != "" {
		trimmed := trimSpace(lineInput)
		if n, err := strconv.Atoi(trimmed); err == nil {
			year = n
		} else {
			ws("年份输入无效或包含非数字字符，将使用默认年份: ")
			writeInt(currentYear)
			wln("。")
		}
	}

	// Get month
	ws("输入月份 (MM) [默认: ")
	writeInt(currentMonth)
	ws(", -1 取消]: ")
	lineInput = readLine()
	if lineInput == "-1" {
		wln("已取消添加开销。")
		return
	}
	month := currentMonth
	if lineInput != "" {
		trimmed := trimSpace(lineInput)
		if n, err := strconv.Atoi(trimmed); err == nil && n >= 1 && n <= 12 {
			month = n
		} else {
			ws("月份输入无效或范围不正确 (1-12)，将使用默认月份: ")
			writeInt(currentMonth)
			wln("。")
		}
	}

	// Get day
	ws("输入日期 (DD) [默认: ")
	writeInt(currentDay)
	ws(", -1 取消]: ")
	lineInput = readLine()
	if lineInput == "-1" {
		wln("已取消添加开销。")
		return
	}
	day := currentDay
	if lineInput != "" {
		trimmed := trimSpace(lineInput)
		if n, err := strconv.Atoi(trimmed); err == nil && n >= 1 && n <= 31 {
			day = n
		} else {
			ws("日期输入无效或范围不正确 (1-31)，将使用默认日期: ")
			writeInt(currentDay)
			wln("。")
		}
	}

	// Basic date validation
	if month < 1 || month > 12 || day < 1 || day > 31 {
		wln("日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。")
		return
	}

	// Get description
	ws("输入描述 (最多 ")
	writeInt(MaxDescriptionLength)
	ws(" 字符, 输入 '!cancel' 取消): ")
	description := readLine()
	if description == "!cancel" {
		wln("已取消添加开销。")
		return
	}
	if len(description) > MaxDescriptionLength {
		ws("描述过长，已截断为 ")
		writeInt(MaxDescriptionLength)
		wln(" 字符。")
		description = truncateString(description, MaxDescriptionLength)
	}

	// Get amount
	ws("输入金额 (-1 取消): ")
	var amount float64
	for {
		lineInput = readLine()
		if lineInput == "-1" {
			wln("已取消添加开销。")
			return
		}
		trimmed := trimSpace(lineInput)
		if a, err := strconv.ParseFloat(trimmed, 64); err == nil && a >= 0 {
			amount = a
			break
		}
		ws("金额无效或为负，请重新输入 (-1 取消): ")
	}

	// Get category
	ws("输入类别 (如 餐饮, 交通, 娱乐; 最多 ")
	writeInt(MaxCategoryLength)
	ws(" 字符, 输入 '!cancel' 取消): ")
	category := readLine()
	if category == "!cancel" {
		wln("已取消添加开销。")
		return
	}
	if len(category) > MaxCategoryLength {
		ws("类别名称过长，已截断为 ")
		writeInt(MaxCategoryLength)
		wln(" 字符。")
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
	wln("开销已添加。")
}

func (t *ExpenseTracker) displayAllExpenses() {
	if t.expenseCount == 0 {
		wln("没有开销记录。")
		return
	}
	wln("\n--- 所有开销记录 ---")
	printExpenseHeader()
	for i := 0; i < t.expenseCount; i++ {
		printExpenseRow(&t.allExpenses[i])
	}
	wln(dash72)
}

func (t *ExpenseTracker) displayMonthlySummary() {
	wln("\n--- 月度开销统计 ---")

	// Get year
	ws("输入要统计的年份 (YYYY) (-1 取消): ")
	var year int
	for {
		n, ok := readInt()
		if ok {
			if n == -1 {
				wln("已取消月度统计。")
				return
			}
			year = n
			break
		}
		ws("年份输入无效，请重新输入 (-1 取消): ")
	}

	// Get month
	ws("输入要统计的月份 (MM) (-1 取消): ")
	var month int
	for {
		n, ok := readInt()
		if ok {
			if n == -1 {
				wln("已取消月度统计。")
				return
			}
			if n >= 1 && n <= 12 {
				month = n
				break
			}
			ws("月份输入无效 (1-12)，请重新输入 (-1 取消): ")
		} else {
			ws("月份输入无效 (1-12)，请重新输入 (-1 取消): ")
		}
	}

	ws("\n--- ")
	writeInt(year)
	ws("年")
	writeInt02(month)
	wln("月 开销统计 ---")

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
		wln("该月份没有开销记录。")
	} else {
		wln(dash72)
		writeStrPad("本月总计:", 62)
		writeFloat2(totalMonthAmount, 10)
		wnl()
		wnl()

		if len(categorySums) > 0 {
			wln("按类别汇总:")
			writeStrPad("类别", 20)
			writeStrPad("总金额", 10)
			wnl()
			wln(dash30)
			for _, cs := range categorySums {
				writeStrPad(cs.Name, 20)
				writeFloat2(cs.Total, 10)
				wnl()
			}
			wln(dash30)
		}
	}
}

func (t *ExpenseTracker) listExpensesByPeriod() {
	for {
		wln("\n--- 按期间列出开销 --- ")
		wln("1. 按年份列出")
		wln("2. 按月份列出")
		wln("3. 按日期列出")
		wln("0. 返回主菜单")
		wln("--------------------")
		ws("请输入选项: ")

		choice, ok := readInt()
		if !ok {
			choice = -1
		}

		switch choice {
		case 1:
			wln("\n--- 按年份列出开销 ---")
			ws("输入年份 (YYYY) (输入 0 返回): ")
			var year int
			for {
				n, ok := readInt()
				if ok {
					year = n
					break
				}
				ws("年份输入无效，请重新输入 (输入 0 返回): ")
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
				ws("在 ")
				writeInt(year)
				wln(" 年没有找到开销记录。")
			}
			wln(dash72)

		case 2:
			wln("\n--- 按月份列出开销 ---")
			ws("输入年份 (YYYY) (输入 0 返回): ")
			var year int
			for {
				n, ok := readInt()
				if ok {
					year = n
					break
				}
				ws("年份输入无效，请重新输入 (输入 0 返回): ")
			}
			if year == 0 {
				continue
			}

			ws("输入月份 (MM) (输入 0 返回): ")
			var month int
			for {
				n, ok := readInt()
				if ok && (n == 0 || (n >= 1 && n <= 12)) {
					month = n
					break
				}
				ws("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ")
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
				ws("在 ")
				writeInt(year)
				ws(" 年 ")
				writeInt(month)
				wln(" 月没有找到开销记录。")
			}
			wln(dash72)

		case 3:
			wln("\n--- 按日期列出开销 ---")
			ws("输入年份 (YYYY) (输入 0 返回): ")
			var year int
			for {
				n, ok := readInt()
				if ok {
					year = n
					break
				}
				ws("年份输入无效，请重新输入 (输入 0 返回): ")
			}
			if year == 0 {
				continue
			}

			ws("输入月份 (MM) (输入 0 返回): ")
			var month int
			for {
				n, ok := readInt()
				if ok && (n == 0 || (n >= 1 && n <= 12)) {
					month = n
					break
				}
				ws("月份输入无效 (1-12)，请重新输入 (输入 0 返回): ")
			}
			if month == 0 {
				continue
			}

			ws("输入日期 (DD) (输入 0 返回): ")
			var day int
			for {
				n, ok := readInt()
				if ok && (n == 0 || (n >= 1 && n <= 31)) {
					day = n
					break
				}
				ws("日期输入无效 (1-31)，请重新输入 (输入 0 返回): ")
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
				ws("在 ")
				writeInt(year)
				ws(" 年 ")
				writeInt(month)
				ws(" 月 ")
				writeInt(day)
				wln(" 日没有找到开销记录。")
			}
			wln(dash72)

		case 0:
			wln("返回主菜单...")
			return

		default:
			wln("无效选项，请重试。")
		}
	}
}

func (t *ExpenseTracker) saveExpenses() {
	f, err := os.OpenFile(DataFile, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		os.Stderr.WriteString("错误：无法打开文件 " + DataFile + " 进行写入！\n")
		return
	}

	// Use a large buffer for file writing
	buf := make([]byte, 0, 32768)

	// Write count
	buf = strconv.AppendInt(buf, int64(t.expenseCount), 10)
	buf = append(buf, '\n')

	for i := 0; i < t.expenseCount; i++ {
		exp := &t.allExpenses[i]
		buf = strconv.AppendInt(buf, int64(exp.Year), 10)
		buf = append(buf, ',')
		buf = strconv.AppendInt(buf, int64(exp.Month), 10)
		buf = append(buf, ',')
		buf = strconv.AppendInt(buf, int64(exp.Day), 10)
		buf = append(buf, ',')
		buf = append(buf, exp.Description...)
		buf = append(buf, ',')
		buf = strconv.AppendFloat(buf, exp.Amount, 'g', -1, 64)
		buf = append(buf, ',')
		buf = append(buf, exp.Category...)
		buf = append(buf, '\n')

		// Flush buffer if getting large
		if len(buf) > 24576 {
			f.Write(buf)
			buf = buf[:0]
		}
	}

	if len(buf) > 0 {
		f.Write(buf)
	}
	f.Close()
}

func (t *ExpenseTracker) loadExpenses() bool {
	f, err := os.Open(DataFile)
	if err != nil {
		return false
	}

	// Read entire file at once
	info, err := f.Stat()
	if err != nil {
		f.Close()
		return false
	}
	size := info.Size()
	if size == 0 {
		f.Close()
		t.expenseCount = 0
		return false
	}

	data := make([]byte, size)
	n, _ := f.Read(data)
	f.Close()
	data = data[:n]

	// Parse using direct byte scanning — no string conversion or splitting
	pos := 0

	// Skip leading whitespace
	for pos < len(data) && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\r') {
		pos++
	}

	// Parse count from first line
	countFromFile := 0
	neg := false
	if pos < len(data) && data[pos] == '-' {
		neg = true
		pos++
	}
	for pos < len(data) && data[pos] >= '0' && data[pos] <= '9' {
		countFromFile = countFromFile*10 + int(data[pos]-'0')
		pos++
	}
	if neg {
		countFromFile = -countFromFile
	}
	// Skip to next line
	for pos < len(data) && data[pos] != '\n' {
		pos++
	}
	if pos < len(data) {
		pos++ // skip '\n'
	}

	if countFromFile < 0 || countFromFile > MaxExpenses {
		t.expenseCount = 0
		return false
	}

	loadedCount := 0
	for lineNum := 0; lineNum < countFromFile && pos < len(data) && loadedCount < MaxExpenses; lineNum++ {
		// Find end of line
		lineEnd := pos
		for lineEnd < len(data) && data[lineEnd] != '\n' {
			lineEnd++
		}
		if lineEnd == pos {
			// empty line, skip
			pos = lineEnd + 1
			continue
		}

		line := data[pos:lineEnd]
		pos = lineEnd + 1

		// Parse 6 comma-separated fields from the line
		fieldStart := 0

		// Field 1: year
		commaIdx := indexByte(line, fieldStart, ',')
		if commaIdx < 0 {
			writeStderr("警告：记录 ", lineNum+1, " 数据不完整 (年份)。\n")
			continue
		}
		year, ok := parseInt(line[fieldStart:commaIdx])
		if !ok {
			continue
		}
		fieldStart = commaIdx + 1

		// Field 2: month
		commaIdx = indexByte(line, fieldStart, ',')
		if commaIdx < 0 {
			writeStderr("警告：记录 ", lineNum+1, " 数据不完整 (月份)。\n")
			continue
		}
		month, ok := parseInt(line[fieldStart:commaIdx])
		if !ok {
			continue
		}
		fieldStart = commaIdx + 1

		// Field 3: day
		commaIdx = indexByte(line, fieldStart, ',')
		if commaIdx < 0 {
			writeStderr("警告：记录 ", lineNum+1, " 数据不完整 (日期)。\n")
			continue
		}
		day, ok := parseInt(line[fieldStart:commaIdx])
		if !ok {
			continue
		}
		fieldStart = commaIdx + 1

		// Field 4: description
		commaIdx = indexByte(line, fieldStart, ',')
		if commaIdx < 0 {
			writeStderr("警告：记录 ", lineNum+1, " 数据不完整 (描述)。\n")
			continue
		}
		descBytes := line[fieldStart:commaIdx]
		descriptionStr := bytesToString(descBytes)
		if len(descriptionStr) > MaxDescriptionLength {
			descriptionStr = truncateString(descriptionStr, MaxDescriptionLength)
		}
		fieldStart = commaIdx + 1

		// Field 5: amount
		commaIdx = indexByte(line, fieldStart, ',')
		if commaIdx < 0 {
			writeStderr("警告：记录 ", lineNum+1, " 数据不完整 (金额)。\n")
			continue
		}
		amountStr := bytesToString(line[fieldStart:commaIdx])
		amount, err := strconv.ParseFloat(amountStr, 64)
		if err != nil {
			continue
		}
		fieldStart = commaIdx + 1

		// Field 6: category (rest of line)
		catBytes := line[fieldStart:]
		// Trim trailing \r
		if len(catBytes) > 0 && catBytes[len(catBytes)-1] == '\r' {
			catBytes = catBytes[:len(catBytes)-1]
		}
		categoryStr := bytesToString(catBytes)
		if len(categoryStr) > MaxCategoryLength {
			categoryStr = truncateString(categoryStr, MaxCategoryLength)
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
	data, err := os.ReadFile(SettlementFile)
	if err != nil {
		return 0, 0
	}
	s := bytesToString(data)
	s = trimSpace(s)

	// Parse two space-separated integers
	spaceIdx := -1
	for i := 0; i < len(s); i++ {
		if s[i] == ' ' || s[i] == '\t' {
			spaceIdx = i
			break
		}
	}
	if spaceIdx < 0 {
		y, _ := strconv.Atoi(s)
		return y, 0
	}
	lastYear, _ := strconv.Atoi(s[:spaceIdx])
	rest := s[spaceIdx+1:]
	rest = trimSpace(rest)
	lastMonth, _ := strconv.Atoi(rest)
	return lastYear, lastMonth
}

func (t *ExpenseTracker) writeLastSettlement(year, month int) {
	buf := make([]byte, 0, 16)
	buf = strconv.AppendInt(buf, int64(year), 10)
	buf = append(buf, ' ')
	buf = strconv.AppendInt(buf, int64(month), 10)
	buf = append(buf, '\n')
	err := os.WriteFile(SettlementFile, buf, 0644)
	if err != nil {
		os.Stderr.WriteString("错误：无法写入结算状态文件 " + SettlementFile + "\n")
	}
}

func (t *ExpenseTracker) generateMonthlyReportForSettlement(year, month int) {
	ws("\n--- ")
	writeInt(year)
	ws("年")
	writeInt02(month)
	wln("月 开销报告 (自动结算) ---")

	totalMonthAmount := 0.0
	foundRecords := false
	categorySums := make([]CategorySum, 0, MaxUniqueCategoriesPerMonth)
	maxCategoryTotal := 0.0
	_ = maxCategoryTotal

	wln("明细:")
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
		wln("该月份没有开销记录。")
		return
	}

	wln(dash72)
	writeStrPad("本月总计:", 62)
	writeFloat2(totalMonthAmount, 10)
	wnl()
	wnl()

	if len(categorySums) > 0 {
		wln("按类别汇总:")
		writeStrPad("类别", 20)
		writeStrPad("总金额", 10)
		wnl()
		wln(dash30)
		for _, cs := range categorySums {
			writeStrPad(cs.Name, 20)
			writeFloat2(cs.Total, 10)
			wnl()
		}
		wln(dash30)
	}

	wln("--- 报告生成完毕 ---")
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
		ws("首次运行或无结算记录，已设置基准结算点为: ")
		writeInt(lastSettledYear)
		ws("年")
		writeInt02(lastSettledMonth)
		wln("月。")
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

		ws("\n>>> 开始自动结算: ")
		writeInt(yearToSettle)
		ws("年")
		writeInt02(monthToSettle)
		wln("月 <<")
		t.generateMonthlyReportForSettlement(yearToSettle, monthToSettle)
		t.writeLastSettlement(yearToSettle, monthToSettle)
		ws(">>> 自动结算完成: ")
		writeInt(yearToSettle)
		ws("年")
		writeInt02(monthToSettle)
		wln("月 <<")
	}
}

func (t *ExpenseTracker) deleteExpense() {
	if t.expenseCount == 0 {
		wln("没有开销记录可供删除。")
		return
	}

	wln("\n--- 删除开销记录 ---")
	wln("以下是所有开销记录:")
	printExpenseHeaderWithIndex()
	for i := 0; i < t.expenseCount; i++ {
		printExpenseRowWithIndex(i+1, &t.allExpenses[i])
	}
	wln(dash77)

	// Get record number to delete
	ws("请输入要删除的记录序号 (0 取消删除): ")
	var recordNumber int
	for {
		n, ok := readInt()
		if ok && n >= 0 && n <= t.expenseCount {
			recordNumber = n
			break
		}
		ws("输入无效。请输入 1 到 ")
		writeInt(t.expenseCount)
		ws(" 之间的数字，或 0 取消: ")
	}

	if recordNumber == 0 {
		wln("取消删除操作。")
		return
	}

	indexToDelete := recordNumber - 1

	wln("\n即将删除以下记录:")
	printExpenseHeader()
	printExpenseRow(&t.allExpenses[indexToDelete])
	wln(dash72)

	// First confirmation
	ws("确认删除吗？ (y/n): ")
	confirm := readLine()

	if len(confirm) > 0 && (confirm[0] == 'y' || confirm[0] == 'Y') {
		// Second confirmation
		wln("\n警告：此操作无法撤销！")
		ws("最后一次确认，真的要删除这条记录吗？ (y/n): ")
		finalConfirm := readLine()

		if len(finalConfirm) > 0 && (finalConfirm[0] == 'y' || finalConfirm[0] == 'Y') {
			wln("\n正在删除记录...")
			for i := indexToDelete; i < t.expenseCount-1; i++ {
				t.allExpenses[i] = t.allExpenses[i+1]
			}
			t.expenseCount--
			wln("记录已删除。")
			t.saveExpenses()
			wln("数据已自动保存。")
		} else {
			wln("已取消删除操作（二次确认未通过）。")
		}
	} else {
		wln("取消删除操作。")
	}
}

// indexByte finds the first occurrence of c in data[start:]
func indexByte(data []byte, start int, c byte) int {
	for i := start; i < len(data); i++ {
		if data[i] == c {
			return i
		}
	}
	return -1
}

// parseInt parses an integer from a byte slice without allocating a string
func parseInt(b []byte) (int, bool) {
	if len(b) == 0 {
		return 0, false
	}
	neg := false
	i := 0
	if b[0] == '-' {
		neg = true
		i++
	}
	n := 0
	for ; i < len(b); i++ {
		if b[i] < '0' || b[i] > '9' {
			return 0, false
		}
		n = n*10 + int(b[i]-'0')
	}
	if neg {
		n = -n
	}
	return n, true
}

// bytesToString converts []byte to string without allocation using unsafe
func bytesToString(b []byte) string {
	if len(b) == 0 {
		return ""
	}
	return *(*string)(unsafe.Pointer(&b))
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
	return b < 0x80 || b >= 0xC0
}

// writeStderr writes a warning to stderr without fmt
func writeStderr(prefix string, num int, suffix string) {
	buf := make([]byte, 0, 64)
	buf = append(buf, prefix...)
	buf = strconv.AppendInt(buf, int64(num), 10)
	buf = append(buf, suffix...)
	os.Stderr.Write(buf)
}

func main() {
	tracker := NewExpenseTracker()
	tracker.run()
}

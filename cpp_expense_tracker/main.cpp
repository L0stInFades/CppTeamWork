#include <iostream>//标准输入输出
#include <fstream>
#include <iomanip>  
#include <ctime>    
#include <cstdlib>  
#include <limits>   
#include <string>   
#include <sstream>   

using namespace std; 

class Expense {
private:
	int year;
	int month;
	int day;
	string description;
	double amount;
	string category;

public:
		// 类内静态常量
	static const size_t MAX_DESCRIPTION_LENGTH = 100; // 描述的逻辑最大长度
	static const size_t MAX_CATEGORY_LENGTH = 50;   // 类别的逻辑最大长度

	/*
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	!!! REFACTORING CANDIDATE / 潜在的重构点: 对象初始化 !!!
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	TODO: 仔细评估 Expense 对象的初始化策略。
	      当前依赖于一个默认构造函数后接 setData() 方法。
	      考虑以下几点:
	      1.  是否应该提供一个或多个参数化构造函数，允许在创建对象时就提供所有必要数据？
	          例如: Expense(int y, int m, int d, const string& desc, double amt, const string& cat)
	      2.  如果提供了参数化构造函数，setData() 的角色是什么？是否主要用于后续修改？
	      3.  默认构造函数创建的对象是否代表一个有效的"空"或"未初始化"状态？其用途是否明确？
	      4.  在构造函数中加入基础的数据校验逻辑，确保对象始终处于一个基本有效的状态。
	      目标: 使对象初始化更直接、更安全，减少对 setData() 的依赖，并确保对象状态的一致性。
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	*/
	// 【开发者提示解读：关于如何创建"开销"对象】
	// 上面这段被 `/* ... */` 包围的、有很多感叹号的注释，是程序员写给其他程序员看的笔记。
	// 它在说："我们创建'一笔开销' (Expense对象) 的方法可能可以做得更好。"
	//
	// 简单来说，现在我们是这样创建一个开销对象的：
	// 1. 先创建一个空的 Expense 对象 (用的是 `Expense()` 这个默认构造函数)。
	// 2. 然后再用 `setData()` 这个函数，把具体的年份、月份、描述、金额等信息填进去。
	//
	// 这段笔记建议思考以下几点，让创建过程更方便、更不容易出错：
	// 1.  能不能在创建 Expense 对象的时候，就一次性把所有信息（年、月、日、描述、金额、类别）都告诉它？
	//     比如，能不能像这样直接创建：`Expense(2023, 10, 26, "午餐", 15.5, "餐饮")`？
	//     这种在创建时就提供所有信息的方法叫做"参数化构造函数"。
	//
	// 2.  如果我们有了这种一次性提供所有信息的方法，那原来的 `setData()` 函数还有什么用呢？
	//     它可能就主要用来修改一个已经存在的开销记录了。
	//
	// 3.  用 `Expense()` 创建的那个"空"对象，它代表的是什么意思？它是不是一个完整有效的开销记录？
	//     这一点需要明确。
	//
	// 4.  在创建对象的时候（无论用哪种方法），最好能立刻检查一下提供的信息是不是基本正确。
	//     比如，月份是不是在1到12之间？金额是不是大于0？这样能保证对象一创建出来就是个合理的状态。
	//
	// 总的目标是：让创建"一笔开销"的过程更直接、更不容易出错，并且确保这个"开销"对象里的信息总是准确的。

	// 【构造函数 - `Expense()`】
	// 构造函数是一种特殊的成员函数，当创建类的一个新对象 (实例) 时，它会自动被调用。
	// 默认构造函数
	Expense() : year(0), month(0), day(0), amount(0.0) {
	
	}

	
	void setData(int y, int m, int d, const string& desc, double amt, const string& cat) {
		year = y;
		month = m;
		day = d;
		description = desc;
		amount = amt;
		category = cat;
        
	}

	// Getter 方法
	int getYear() const { return year; }
	int getMonth() const { return month; }
	int getDay() const { return day; }
	const string& getDescription() const { return description; }
	double getAmount() const { return amount; }
	const string& getCategory() const { return category; }
};

class CategorySum {
public:
	string name; // C-style char array to string
	double total;

	CategorySum() : total(0.0) { // 默认构造函数
		// string name 会自动初始化为空字符串
	}
};


const int MAX_EXPENSES = 1000; 
const int MAX_UNIQUE_CATEGORIES_PER_MONTH = 20;
const char* DATA_FILE = "expenses.dat";
const char* SETTLEMENT_FILE = "settlement_status.txt";

class ExpenseTracker {
private:
	Expense allExpenses[MAX_EXPENSES]; // Expense 对象数组
	int expenseCount;                  // 当前开销数量

	// 私有辅助方法
	void clearInputBuffer();
	void readLastSettlement(int& lastYear, int& lastMonth);
	void writeLastSettlement(int year, int month);
	void generateMonthlyReportForSettlement(int year, int month);

public:
	ExpenseTracker();  // 构造函数
	~ExpenseTracker(); // 析构函数 (可选, 此处为空)

	void run(); // 运行主程序循环

	// 公共接口方法
	void addExpense();
	void displayAllExpenses();
	void displayMonthlySummary();
	// void generateSimpleChart(); // Removed
	void listExpensesByPeriod(); // Added
	void saveExpenses();
	bool loadExpenses();
	void deleteExpense();
	void performAutomaticSettlement();
};

// --- ExpenseTracker 类成员函数实现 ---
/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!! CRITICAL REFACTORING POINT / 核心重构点: MVC 架构分离 !!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
TODO: UI逻辑、业务逻辑、控制逻辑必须分离！采用MVC (Model-View-Controller) 设计模式。

      目标是将当前混合的逻辑拆分成三个明确职责的组件:

      1. M - Model (模型层 - 例如: 新建类 `ExpenseService` 或 `ExpenseModel`):
         职责:
         -   管理应用程序的核心数据 (例如: `allExpenses` 数组, `expenseCount`) 和状态。
         -   包含所有的业务规则和数据处理逻辑 (例如: 添加、删除、查询开销的实际操作，数据验证，计算统计，文件保存与加载，自动结算逻辑)。
         -   完全独立于用户界面。Model 不知道数据将如何显示，也不知道用户如何输入数据。
         -   提供清晰的接口供 Controller 调用 (例如: `bool createExpense(data)`, `vector<Expense> getAllExpenses()`, `bool saveExpensesToFile()`, `MonthlySummaryData generateMonthlySummary(year, month)`)。
         -   示例: `DATA_FILE`, `SETTLEMENT_FILE`, `MAX_EXPENSES` 这些常量和相关文件操作逻辑应属于 Model。

      2. V - View (视图层 - 例如: 新建类 `ExpenseTrackerConsoleUI`):
         职责:
         -   负责所有与用户的直接交互，即输入和输出。
         -   显示数据给用户 (例如: 打印菜单、列表、报告、提示信息、错误信息)。
         -   从用户获取输入 (例如: 读取菜单选项、开销详情)。
         -   不包含任何业务逻辑。它只是一个"哑"的界面，根据 Controller 的指令显示信息或收集输入。
         -   提供接口供 Controller 调用 (例如: `int displayMainMenu()`, `ExpenseData promptForNewExpenseData()`, `void displayExpenses(const vector<Expense>& expenses)`, `void showMessage(const string& message)`)。

      3. C - Controller (控制器层 - 当前的 `ExpenseTracker` 类将演变为此角色):
         职责:
         -   作为 Model 和 View 之间的协调者和中介。
         -   接收来自 View 的用户请求/输入 (例如，用户选择了"添加开销"菜单项)。
         -   根据用户请求调用 Model 的相应方法来处理数据或执行业务逻辑。
         -   获取 Model 处理后的结果。
         -   选择合适的 View 来更新显示，将结果呈现给用户。
         -   管理应用程序的流程和状态转换。
         -   示例: `ExpenseTracker::run()` 方法将是主要的控制循环。当用户选择"添加开销"时：
             a. `Controller::run()` 调用 `View::promptForNewExpenseData()` 获取用户输入的开销信息。
             b. 如果数据有效, `Controller` 调用 `Model::createExpense(expenseData)`。
             c. `Model` 执行添加逻辑，返回成功/失败状态。
             d. `Controller` 根据 `Model` 的返回，调用 `View::showMessage()` 显示相应信息。

      重构步骤建议:
      A. 先定义 `ExpenseService` (Model) 和 `ExpenseTrackerConsoleUI` (View) 的接口和基本骨架。
      B. 逐步将 `ExpenseTracker` 方法中的业务逻辑和数据存储迁移到 `ExpenseService`。
      C. 逐步将 `ExpenseTracker` 方法中的 `cout` 和 `cin` 迁移到 `ExpenseTrackerConsoleUI`。
      D. 修改 `ExpenseTracker` (Controller) 的方法，使其调用新的 Model 和 View 完成任务。

      这样做的好处:
      -   高内聚，低耦合：每个组件职责单一，修改一个组件不轻易影响其他组件。
      -   可维护性：逻辑清晰，更容易理解和修改。
      -   可测试性：Model 可以独立于 UI 进行单元测试。
      -   灵活性：未来如果想更换 UI (例如图形界面)，理论上只需替换 View 层。
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

// 【开发者提示解读：关于代码的"整理房间"——MVC模式】
// 上面这段非常长的、被 `/* ... */` 和很多感叹号包围的注释，是程序员写给其他程序员看的，关于如何把代码写得更整洁、更有条理的专业建议。
// 它在说："我们这个开销追踪程序，现在把所有事情都放在一个大抽屉 (`ExpenseTracker` 类) 里了，以后东西多了可能会乱。我们应该学习一种叫做 MVC 的整理方法。"
//
// MVC 是什么？
// 想象一下餐馆的运作：
// -   Model (模型 M - 管理数据和规则)：就像是餐馆的【后厨】。负责管理食材 (数据)，处理订单 (业务逻辑)，比如计算总价、保存记录等。后厨不直接跟顾客打交道。
// -   View (视图 V - 展示给用户看/用户操作的界面)：就像是餐馆的【菜单和餐桌上的食物】。负责把菜品 (数据) 好看地展示给顾客，以及记录顾客点的菜 (用户输入)。菜单本身不做菜。
// -   Controller (控制器 C - 指挥官)：就像是餐馆的【服务员】。服务员在顾客和后厨之间跑腿。他把顾客的点单 (来自View的请求)告诉后厨 (调用Model)，然后把做好的菜 (来自Model的结果)端给顾客 (更新View)。
//
// 这段注释建议我们把 `ExpenseTracker` 程序也这样分开：
// 1.  **Model (模型层)**：专门负责所有的数据处理。比如 `allExpenses` 数组、`expenseCount` 变量，还有保存文件、从文件加载、计算月度总结这些具体的数据操作，都应该放到 Model 里。它不关心这些数据怎么显示给用户。
// 2.  **View (视图层)**：专门负责和用户打交道。所有 `cout` (在屏幕上显示东西) 和 `cin` (从键盘读取东西) 的代码，比如显示菜单、打印开销列表、提示用户输入等，都应该放到 View 里。它不处理具体的数据逻辑。
// 3.  **Controller (控制器层)**：现在的 `ExpenseTracker` 类慢慢变成这个角色。它会接收用户的操作 (比如用户选了"添加开销")，然后告诉 Model 去做相应的数据处理，拿到结果后，再告诉 View 把结果显示出来。
//
// 为什么要这么做（好处）？
// -   **代码更清晰**：每个部分只做自己的事，就像餐馆里厨师专心做菜，服务员专心服务一样。找问题、改东西都更容易。
// -   **更容易维护和修改**：如果想改菜单样式 (View)，不用动后厨 (Model) 的代码。如果想改菜品价格计算方法 (Model)，顾客看到的菜单 (View) 可能不用变。
// -   **更容易测试**：可以单独测试后厨 (Model) 的功能好不好用，不用真的找个顾客来点单。
// -   **更灵活**：以后如果想把现在的命令行界面换成漂亮的图形窗口界面，理论上只需要换掉 View 部分，Model 和 Controller 可能不用大改。
//
// 对于初学者：
// 这是一种让代码结构更棒的设计思想。目前程序的功能是正常的。等你对C++更熟悉了，学习这种设计模式会对编写大型程序非常有帮助。
// 现在，我们可以继续看 `ExpenseTracker` 类里的函数是怎么具体工作的。

// 【ExpenseTracker 构造函数实现】
// `ExpenseTracker::` // 这个双冒号叫做作用域解析运算符，它表明我们现在定义的是属于 `ExpenseTracker` 类的那个名为 `ExpenseTracker` 的函数（也就是构造函数）。
// `: expenseCount(0)` // 这是成员初始化列表。在构造函数体执行之前，它会把成员变量 `expenseCount` 初始化为0。
//                   // 对于类来说，这是一种推荐的初始化成员变量的方式，比在函数体内部赋值更高效。
ExpenseTracker::ExpenseTracker() : expenseCount(0) {
	// `if (loadExpenses())` // 调用本类 (ExpenseTracker对象自己) 的 `loadExpenses()` 方法，尝试从文件中加载已保存的开销数据。
	                       // `loadExpenses()` 会返回一个布尔值 (`true` 或 `false`)。
	if (loadExpenses()) { // 如果 `loadExpenses()` 返回 `true` (表示数据成功加载)
		// `cout` // 是 `iostream` 库中提供的标准输出流对象，通常用于向控制台（屏幕）输出信息。
		// `<<`  // 是流插入运算符，它把右边的内容发送到左边的流中。
		// `expenseCount` // 此处是成员变量 `expenseCount`，它在 `loadExpenses()` 成功后会被更新为加载的记录条数。
		// `" 条历史记录。\n"` // 这是一个字符串字面量。`\n` 是一个转义字符，代表换行，使后续输出从新的一行开始。
		cout << "成功加载 " << expenseCount << " 条历史记录。\n"; // 在屏幕上打印加载成功的消息和记录数量。
	} else { // `else` 分支：如果 `loadExpenses()` 返回 `false` (表示加载失败，比如文件不存在或文件内容损坏)
		cout << "未找到历史数据文件或加载失败，开始新的记录。\n"; // 在屏幕上打印相应的提示信息。
	}
	// `performAutomaticSettlement()` // 调用本类的 `performAutomaticSettlement()` 方法。
	                               // 这个方法用于检查并自动处理（例如生成报告）过去月份中尚未结算的开销数据。
	                               // 这样程序每次启动时，都会确保历史数据的完整性。
	performAutomaticSettlement(); // 执行自动结算检查。
} // 构造函数结束

// 【ExpenseTracker 析构函数实现】
// `~ExpenseTracker()` // 这是 `ExpenseTracker` 类的析构函数。析构函数在对象生命周期结束时（例如，`main`函数中的`tracker`对象在`main`函数结束时）自动被调用。
                    // 它的主要用途是释放对象在生命周期内可能获取的资源（如动态分配的内存、打开的文件等）。
                    // 析构函数的名称是类名前面加一个波浪号 `~`，并且没有参数，也没有返回类型。
ExpenseTracker::~ExpenseTracker() {
	// 这个析构函数的函数体是空的。
	// 在这个特定的程序中，数据保存是在用户选择退出时通过 `saveExpenses()` 方法显式完成的，
	// 并且没有在 `ExpenseTracker` 对象内部直接使用 `new` 动态分配需要 `delete` 清理的内存。
	// 因此，目前这个析构函数不需要执行特定的清理操作。
	// 如果将来类变得更复杂，比如动态管理了某些资源，就需要在这里添加相应的释放代码。
} // 析构函数结束

// 【私有辅助方法 `clearInputBuffer` 实现】
// `void` // 表示这个函数不返回任何值。
// `ExpenseTracker::clearInputBuffer()` // 定义 `ExpenseTracker` 类的 `clearInputBuffer` 方法。
                                     // 这是一个私有方法，意味着它主要被类内部的其他成员函数调用，用于辅助完成任务。
void ExpenseTracker::clearInputBuffer() {
	// `cin` // 标准输入流对象，通常代表键盘输入。
	// `.ignore(...)` // 是输入流的一个成员函数，用于忽略（跳过并丢弃）输入流中的字符。
	// `numeric_limits<streamsize>::max()` // `numeric_limits` 是定义在 `<limits>` 头文件中的模板类，用于获取各种数据类型的属性（如此处的最大值）。
	                                     // `streamsize` 是一种用于表示流大小的整数类型。
	                                     // `numeric_limits<streamsize>::max()` 返回 `streamsize` 类型能表示的最大值。
	                                     // 这意味着 `.ignore()` 会尝试忽略非常非常多的字符，直到遇到下一个参数指定的停止条件。
	// `'\n'` // 这是第二个参数，指定了忽略操作的终止字符。`'\n'` 代表换行符。
	//         // 当用户在控制台输入并按下回车键时，这个换行符也会被放入输入缓冲区。
	// 这整行代码的作用是：丢弃输入缓冲区中当前位置直到下一个换行符（包括该换行符）之间的所有字符，或者丢弃掉缓冲区内所有剩余字符（如果先达到了流的最大限制）。
	// 这样做的目的是为了清除在一次输入操作（尤其是使用 `>>` 操作符读取数字后）后可能残留在缓冲区中的换行符或其他多余字符，
	// 以防止它们影响到后续的输入操作（特别是 `getline` 函数，它会读取整行直到换行符）。
	cin.ignore(numeric_limits<streamsize>::max(), '\n'); // 清除输入缓冲区中直到下一个换行符的所有内容。
} // `clearInputBuffer` 函数结束

// 【核心功能方法 `run` 实现】
// `void ExpenseTracker::run()` // 定义 `ExpenseTracker` 类的 `run` 方法。这是程序的用户交互核心。
void ExpenseTracker::run() {
	int choice; // 声明一个整型变量 `choice`，用于存储用户从菜单中选择的选项的数字。
	// `do...while` // 这是一个 `do-while` 循环结构。它的特点是循环体内的代码至少会执行一次，然后在每次循环结束时检查 `while` 后面的条件。
	             // 只要条件为真，循环就会继续。
	do { // `do-while` 循环开始
		// 下面是一系列的 `cout` 语句，用于在控制台（屏幕）上打印出程序的主菜单。
		cout << "\n大学生开销追踪器\n"; // `\n` 是换行符，使标题在新的一行显示，并且前面有一个空行，让界面更清晰。
		cout << "--------------------\n"; // 打印一行分隔线。
		cout << "1. 添加开销记录\n";   // 菜单选项1。
		cout << "2. 查看所有开销\n";   // 菜单选项2。
		cout << "3. 查看月度统计\n";   // 菜单选项3。
		cout << "4. 按期间列出开销\n"; // 菜单选项4。
		cout << "5. 删除开销记录\n";   // 菜单选项5。
		cout << "6. 保存并退出\n";     // 菜单选项6。
		cout << "--------------------\n"; // 打印一行分隔线。
		cout << "请输入选项: ";        // 提示用户输入他们的选择。

		// `cin >> choice;` // 使用标准输入流 `cin` 和提取运算符 `>>` 来读取用户输入的整数，并将其存储到 `choice` 变量中。
		                    // 注意：`>>` 操作符在读取数字时，会跳过前面的空白字符（空格、制表符、换行符），然后读取数字字符，
		                    // 直到遇到第一个非数字字符。它会将这个非数字字符（通常是按下回车时产生的换行符 `\n`）留在输入缓冲区中。
		cin >> choice; // 从用户那里获取菜单选项。

		// 【输入验证和错误处理】
		// `cin.fail()` // 这个成员函数检查输入流 `cin` 是否处于"失败"状态。
		              // 如果上一次输入操作（如 `cin >> choice`）因为用户输入了无效的内容（例如输入了字母而不是数字）而失败，`cin.fail()` 会返回 `true`。
		if (cin.fail()) { // 如果输入操作失败了 (例如，用户输入了文本而不是数字)
			cin.clear();          // `cin.clear()`: 这个成员函数用于清除输入流的错误状态标志（例如 `failbit` 或 `badbit`）。
			                      // 清除错误标志后，输入流对象才能恢复正常工作，进行下一次输入/输出操作。
			clearInputBuffer();   // 调用我们自己定义的 `clearInputBuffer()` 辅助函数，
			                      // 来清除输入缓冲区中导致先前输入失败的无效字符 (以及随后的换行符)。
			choice = 0;           // 将 `choice` 变量的值设置为0。这是一个任意选择的无效菜单选项值，
			                      // 目的是确保在接下来的 `switch` 语句中，程序会进入 `default` 分支，从而提示用户输入无效。
		} else { // `else` 分支：如果 `cin >> choice` 操作没有失败 (即用户确实输入了看起来像数字的内容)
			clearInputBuffer();   // 即使输入成功，输入缓冲区中通常还残留着用户按下回车键时产生的换行符 `\n`。
			                      // 调用 `clearInputBuffer()` 来清除这个换行符，对于后续可能使用 `getline`（它对换行符敏感）的函数调用来说，这是个好习惯。
		} // 输入验证结束。

		// `switch (choice)` // `switch` 语句是一种多分支选择结构。它会根据 `choice` 变量的值，跳转到匹配的 `case`标签处执行代码。
		switch (choice) { // 根据用户的选择执行相应的操作。
		case 1: // 如果 `choice` 的值是 1
			addExpense(); // 调用 `addExpense()` 成员方法来处理添加新开销的逻辑。
			break;        // `break;` 语句使程序跳出当前的 `switch` 结构，不再检查后续的 `case`。
		              // 如果没有 `break;`，程序会继续执行下一个 `case` 中的代码（称为"穿透"）。
		case 2: // 如果 `choice` 的值是 2
			displayAllExpenses(); // 调用 `displayAllExpenses()` 成员方法来显示所有开销记录。
			break; // 跳出 `switch`。
		case 3: // 如果 `choice` 的值是 3
			displayMonthlySummary(); // 调用 `displayMonthlySummary()` 成员方法来显示月度开销统计。
			break; // 跳出 `switch`。
		case 4: // 如果 `choice` 的值是 4
			listExpensesByPeriod(); // 调用 `listExpensesByPeriod()` 成员方法来按指定时段列出开销。
			break; // 跳出 `switch`。
		case 5: // 如果 `choice` 的值是 5
			deleteExpense(); // 调用 `deleteExpense()` 成员方法来删除指定的开销记录。
			break; // 跳出 `switch`。
		case 6: // 如果 `choice` 的值是 6 (用户选择保存并退出)
			saveExpenses(); // 调用 `saveExpenses()` 成员方法，将当前的开销数据保存到文件中。
			cout << "数据已保存。正在退出...\n"; // 向用户显示一条消息，表明数据已保存并且程序即将退出。
			break; // 跳出 `switch`。
		default: // `default` 分支：如果 `choice` 的值不匹配前面任何一个 `case` (例如，用户输入了无效数字，或者输入错误被处理后 `choice` 被设为0)
			cout << "无效选项，请重试。\n"; // 向用户显示错误提示，要求他们重新输入一个有效的选项。
			// 此处没有 `break;` 因为 `default` 通常是 `switch` 语句的最后一个分支，执行完后自然会退出 `switch`。
		} // `switch` 语句结束。
	} while (choice != 6); // `do-while` 循环的条件判断：只要用户的选择 `choice` 不是 6 (退出选项)，循环就会继续，重新显示菜单并等待用户输入。
	                       // 当 `choice` 等于 6 时，条件为 `false`，循环终止，`run()` 函数结束，程序流程返回到调用 `run()` 的地方 (即 `main` 函数)。
} // `run` 函数结束。

// 【`addExpense` 方法实现 - 添加新的开销记录】
// `void ExpenseTracker::addExpense()` // 定义 `ExpenseTracker` 类的 `addExpense` 成员方法。
void ExpenseTracker::addExpense() {
	// 【检查容量是否已满】
	// `if (expenseCount >= MAX_EXPENSES)` // 检查当前已存储的开销数量 `expenseCount` 是否已经达到或超过了预设的最大容量 `MAX_EXPENSES`。
	                                    // `MAX_EXPENSES` 是一个全局常量，定义了 `allExpenses` 数组的大小。
	if (expenseCount >= MAX_EXPENSES) { // 如果记录已满
		cout << "错误：开销记录已满！无法添加更多记录。\n"; // 打印错误消息。
		return; // `return;` 语句立即从当前函数 (`addExpense`) 返回，不再执行后续的添加逻辑。
	} // 容量检查结束。

	// 【声明用于存储用户输入的局部变量】
	int year, month, day;    // `int` 类型变量，分别用于存储用户输入的年份、月份和日期。
	string description;      // `string` 类型变量 (来自 `<string>` 库)，用于存储开销的文字描述。
	double amount;           // `double` 类型变量，用于存储开销的金额 (可以包含小数)。
	string category;         // `string` 类型变量，用于存储开销的类别。
	string line_input;       // `string` 类型变量，用于临时存储用户通过 `getline` 输入的一整行文本。
	                         // 使用它是因为 `getline` 可以读取包含空格的输入，而 `cin >> ...` 遇到空格会停止。

	cout << "\n--- 添加新开销 (输入 '-1' 作为数字或 '!cancel' 作为文本可取消) ---\n"; // 打印一个提示标题，告知用户如何取消操作。

	// 【获取当前系统日期，用作后续输入的默认值】
	// `time_t` // 是一种算术类型，用于表示时间，通常是自某个固定点（如1970年1月1日 UTC）以来的秒数。
	// `now`    // 声明一个 `time_t` 类型的变量 `now`。
	// `time(0)`// `time()` 函数 (来自 `<ctime>` 头文件) 返回当前日历时间。参数 `0` (或 `nullptr`) 是常见的用法。
	time_t now = time(0); // 获取当前时间的秒数表示。
	// `tm`       // 是一个结构体 (定义在 `<ctime>` 中)，用于存储日历时间的各个组成部分 (年、月、日、时、分、秒等)。
	// `*ltm`     // 声明一个指向 `tm` 结构体的指针 `ltm`。
	// `localtime(&now)` // `localtime()` 函数将 `time_t` 类型的时间（`now` 的地址 `&now` 所指向的值）转换为本地时间的 `tm` 结构体形式。
	                    // 它返回一个指向内部静态分配的 `tm` 结构体的指针。
	tm *ltm = localtime(&now); // 将秒数转换为本地时间的 `tm` 结构体。
	// 从 `tm` 结构体中提取当前的年、月、日信息，用于默认值。
	// `ltm->tm_year` // `tm` 结构体中的 `tm_year` 成员表示的是从1900年开始的年数。
	int currentYear = 1900 + ltm->tm_year; // 所以需要加上1900得到实际的公元年份。
	// `ltm->tm_mon`  // `tm_mon` 成员表示月份，范围是 0 (一月) 到 11 (十二月)。
	int currentMonth = 1 + ltm->tm_mon;    // 所以需要加1得到实际的月份 (1-12)。
	// `ltm->tm_mday` // `tm_mday` 成员表示月份中的日期，范围是 1 到 31。
	int currentDay = ltm->tm_mday;       // 这个值可以直接使用。

	// 【获取年份输入】
	// `cout << ...`: 打印提示信息，包括当前的默认年份以及如何取消。
	cout << "输入年份 (YYYY) [默认: " << currentYear << ", -1 取消]: ";
	// `getline(cin, line_input);` // 使用 `getline` 函数从标准输入 `cin` 读取一整行用户输入 (直到用户按下回车)，
	                               // 并将读取的内容存储到 `line_input` 字符串变量中。
	                               // 这与 `cin >> year;` 不同，`getline` 可以读取包含空格的行，并且会读取并丢弃行尾的换行符。
	getline(cin, line_input); // 读取用户输入的整行年份信息。
	// `if (line_input == "-1")`: 检查用户是否输入了 "-1" 来取消操作。
	if (line_input == "-1") { cout << "已取消添加开销。\n"; return; } // 如果是，打印取消消息并从函数返回。
	// `if (!line_input.empty())`: 检查用户是否输入了内容 (即 `line_input` 字符串不是空的)。
	                             // 如果用户只是直接按了回车，`line_input` 会是空的。
	if (!line_input.empty()) { // 如果用户输入了非空内容
		// `stringstream` // (来自 `<sstream>` 头文件) 允许你像处理输入/输出流 (`cin`/`cout`) 一样处理内存中的字符串。
		// `ss(line_input)` // 创建一个名为 `ss` 的 `stringstream` 对象，并将用户输入的 `line_input` 字符串的内容放入这个流中。
		stringstream ss(line_input); // 用用户输入行创建一个字符串流。
		// `!(ss >> year)` // 尝试从字符串流 `ss` 中提取一个整数到 `year` 变量。
		                 // 如果提取失败 (例如，`line_input` 中包含非数字字符，或者为空)，这个表达式的结果为 `true`。
		// `!ss.eof()`   // `eof()` (end-of-file) 成员函数检查流是否到达了末尾。
		                 // `!ss.eof()` 为 `true` 意味着在成功提取整数 `year` 之后，字符串流 `ss` 中还有剩余的未被读取的非空白字符。
		                 // 我们希望年份输入是纯数字，所以这种情况也视为无效。
		if (!(ss >> year) || !ss.eof()) { // 如果提取年份失败，或者提取后仍有非数字残留
			cout << "年份输入无效或包含非数字字符，将使用默认年份: " << currentYear << "。\n"; // 打印错误消息。
			year = currentYear; // 将 `year` 设置为之前获取的当前系统年份作为默认值。
		} // 年份解析和验证结束。
	} else { // `else` 分支：如果 `line_input.empty()` 为 `true` (即用户直接按了回车，没有输入任何内容)
		year = currentYear; // 使用当前系统年份作为默认值。
	} // 年份输入处理完毕。

	// 【获取月份输入】 - 这部分逻辑与获取年份输入的逻辑非常相似。
	cout << "输入月份 (MM) [默认: " << currentMonth << ", -1 取消]: "; // 提示用户输入月份。
	getline(cin, line_input); // 读取整行输入。
	if (line_input == "-1") { cout << "已取消添加开销。\n"; return; } // 处理取消操作。
	if (!line_input.empty()) { // 如果输入非空
		stringstream ss(line_input); // 创建字符串流。
		if (!(ss >> month) || !ss.eof() || month < 1 || month > 12) { // 尝试提取月份，并增加月份范围 (1-12) 的校验。
		                                                              // 如果提取失败，或有残留，或月份不在1-12范围内，则视为无效。
			cout << "月份输入无效或范围不正确 (1-12)，将使用默认月份: " << currentMonth << "。\n"; // 打印错误消息。
			month = currentMonth; // 使用当前系统月份作为默认值。
		} // 月份解析和验证结束。
	} else { // 如果用户直接回车
		month = currentMonth; // 使用当前系统月份作为默认值。
	} // 月份输入处理完毕。

	// 【获取日期输入】 - 逻辑与获取年份/月份相似，但校验条件略有不同。
	cout << "输入日期 (DD) [默认: " << currentDay << ", -1 取消]: "; // 提示用户输入日期。
	getline(cin, line_input); // 读取整行输入。
	if (line_input == "-1") { cout << "已取消添加开销。\n"; return; } // 处理取消操作。
	if (!line_input.empty()) { // 如果输入非空
		stringstream ss(line_input); // 创建字符串流。
		// 尝试提取日期，并进行基础的范围校验 (1-31)。注意，这里没有根据具体月份和年份来校验日期的最大值 (例如2月不能有30号)。
		// 在下面的开发者注释（TODO块）中也指出了这一点需要改进。
		if (!(ss >> day) || !ss.eof() || day < 1 || day > 31) { // 如果提取失败，或有残留，或日期不在1-31范围内
			cout << "日期输入无效或范围不正确 (1-31)，将使用默认日期: " << currentDay << "。\n"; // 打印错误消息。
			day = currentDay; // 使用当前系统日期作为默认值。
		} // 日期解析和验证结束。
	} else { // 如果用户直接回车
		day = currentDay; // 使用当前系统日期作为默认值。
	} // 日期输入处理完毕。

	/* 【开发人员的注释 - 关于日期校验的重要提示】
	   (这部分注释在之前的步骤中已经由我（AI助手）添加了初学者解读，这里是原始的开发者注释)
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	!!! CRITICAL VALIDATION POINT / 必须实现的日期校验逻辑 !!!
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	TODO: 此处必须实现严格的日期有效性检查！
	      当前的验证 (month < 1 || month > 12 || day < 1 || day > 31) 非常基础。
	      需要加入以下检查:
	      1.  根据年份和月份，判断 'day' 是否在该月份的有效范围内 (例如，4月不能有31日)。
	      2.  处理闰年情况 (2月份的天数)。
	      3.  可以考虑不允许输入未来的日期，或者至少给出警告。
	      4.  确保年份的合理性（例如，不是公元0年或公元9999年这种极端情况）。
	      重要: 如果日期无效，应持续提示用户重新输入，直到用户提供有效日期为止。
            绝对不能像当前这样直接回退到默认日期！当前的默认回退行为必须修改。
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	*/
	// 【基础日期有效性二次确认】
	// 在分别处理完年、月、日的默认值逻辑后，这里再对组合后的日期进行一次非常基础的有效性检查。
	// 这与上面在输入月份和日期时各自进行的范围检查是部分重叠的，但可以看作一个最终确认。
	// 理想情况下，这里应该有一个更完善的日期验证函数，如TODO注释所建议。
	if (month < 1 || month > 12 || day < 1 || day > 31) { // 再次检查月份是否在1-12之间，日期是否在1-31之间。
		cout << "日期输入无效（例如月份不在1-12，或日期不在1-31），请重新输入。\n"; // 打印错误消息。
		// 注意：根据TODO注释的建议，这里不应该直接返回，而是应该提示用户重新输入整个日期，或者至少重新输入有问题的部分。
		// 当前的实现是如果日期组合后仍不符合这个基础检查，则直接放弃本次添加操作。
		return; // 从 `addExpense` 函数返回，添加失败。
	} // 基础日期验证结束。

	// 【获取描述输入】
	// `Expense::MAX_DESCRIPTION_LENGTH` // 访问 `Expense` 类中定义的静态常量 `MAX_DESCRIPTION_LENGTH`，它规定了描述的最大字符数。
	cout << "输入描述 (最多 " << Expense::MAX_DESCRIPTION_LENGTH << " 字符, 输入 '!cancel' 取消): "; // 提示用户输入描述。
	getline(cin, description); // 使用 `getline` 读取用户输入的描述，允许包含空格。
	if (description == "!cancel") { cout << "已取消添加开销。\n"; return; } // 处理取消操作。
	// `description.length()` // `string` 对象的 `length()` 成员函数返回字符串中的字符数量。
	if (description.length() > Expense::MAX_DESCRIPTION_LENGTH) { // 如果用户输入的描述长度超过了允许的最大长度
		cout << "描述过长，已截断为 " << Expense::MAX_DESCRIPTION_LENGTH << " 字符。\n"; // 打印提示消息。
		// `description.substr(0, Expense::MAX_DESCRIPTION_LENGTH)` // `string` 对象的 `substr()` 成员函数用于提取子字符串。
		                                                          // 参数 `0` 是起始位置，`Expense::MAX_DESCRIPTION_LENGTH` 是要提取的长度。
		                                                          // 这会将描述截断到允许的最大长度。
		description = description.substr(0, Expense::MAX_DESCRIPTION_LENGTH); // 执行截断。
	} // 描述长度处理完毕。

	// 【获取金额输入】
	cout << "输入金额 (-1 取消): "; // 提示用户输入金额。
	// `while (true)` // 创建一个无限循环。这个循环会一直执行，直到遇到 `break` (输入有效) 或 `return` (用户取消)。
	while (true) { // 开始金额输入循环
		getline(cin, line_input); // 读取用户输入的整行金额信息。
		if (line_input == "-1") { cout << "已取消添加开销。\n"; return; } // 处理取消操作。

		stringstream ss_amount(line_input); // 用用户输入行创建一个字符串流 `ss_amount`。
		// `ss_amount >> amount` // 尝试从字符串流中提取一个 `double` 类型的值到 `amount` 变量。
		// `amount >= 0`         // 检查提取出的金额是否大于或等于0 (即非负数)。
		// `ss_amount.eof()`     // 检查在提取金额后，字符串流是否已经到达末尾 (确保输入的是纯数字，没有多余字符)。
		if (ss_amount >> amount && amount >= 0 && ss_amount.eof()) { // 如果金额成功提取、非负，并且没有多余字符
			break; // `break;` 跳出当前的 `while (true)` 循环，因为已获得有效金额。
		} // 有效金额判断结束。
		// 如果上面的 `if` 条件不满足 (即金额无效)
		cout << "金额无效或为负，请重新输入 (-1 取消): "; // 打印错误消息，提示用户重新输入。
	} // `while (true)` 循环结束，此时 `amount` 中已存储有效金额。

	// 【获取类别输入】
	cout << "输入类别 (如 餐饮, 交通, 娱乐; 最多 " << Expense::MAX_CATEGORY_LENGTH << " 字符, 输入 '!cancel' 取消): "; // 提示输入类别。
	getline(cin, category); // 使用 `getline` 读取用户输入的类别，允许包含空格。
	if (category == "!cancel") { cout << "已取消添加开销。\n"; return; } // 处理取消操作。
	// `category.length()` // 获取类别字符串的长度。
	if (category.length() > Expense::MAX_CATEGORY_LENGTH) { // 如果类别长度超过了 `Expense` 类中定义的 `MAX_CATEGORY_LENGTH`
		cout << "类别名称过长，已截断为 " << Expense::MAX_CATEGORY_LENGTH << " 字符。\n"; // 打印提示消息。
		category = category.substr(0, Expense::MAX_CATEGORY_LENGTH); // 将类别截断到允许的最大长度。
	} // 类别长度处理完毕。
	// `category.empty()` // `string` 对象的 `empty()` 成员函数检查字符串是否为空 (即长度为0)。
	if (category.empty()) { // 如果用户没有输入任何类别内容 (例如直接按了回车)
		category = "未分类"; // 将类别设置为默认值 "未分类"。
	} // 空类别处理完毕。

	// 【将收集到的数据存储到开销对象数组中】
	// `allExpenses` // 是 `ExpenseTracker` 类中定义的 `Expense` 对象数组。
	// `expenseCount`// 是当前数组中已有的开销记录数量，也恰好是下一个新记录应存放的索引位置 (因为数组索引从0开始)。
	// `allExpenses[expenseCount]` // 通过索引访问数组中的一个 `Expense` 对象。
	// `.setData(...)` // 调用这个 `Expense` 对象的 `setData` 成员方法，
	                // 将用户输入的年、月、日、描述、金额、类别等信息传递给它，用于设置该对象的内部状态。
	allExpenses[expenseCount].setData(year, month, day, description, amount, category); // 设置新开销记录的数据。
	// `expenseCount++` // 将 `expenseCount` 的值加1。这表示开销记录的总数增加了一个，
	                  // 并且也为下一次添加新记录时 `expenseCount` 能指向正确的下一个空位做好了准备。
	expenseCount++; // 增加已存储的开销数量。
	cout << "开销已添加。\n"; // 打印成功添加的消息。
} // `addExpense` 函数结束。

// 【`displayAllExpenses` 方法实现 - 显示所有开销记录】
// `void ExpenseTracker::displayAllExpenses()` // 定义 `ExpenseTracker` 类的 `displayAllExpenses` 成员方法。
void ExpenseTracker::displayAllExpenses() {
	// 【检查是否有记录可显示】
	if (expenseCount == 0) { // 如果当前没有任何开销记录 (`expenseCount` 为 0)
		cout << "没有开销记录。\n"; // 打印提示信息。
		return; // 从函数返回，不再执行后续的显示逻辑。
	} // 记录检查结束。
	cout << "\n--- 所有开销记录 ---\n"; // 打印列表的标题。
	// 【打印表头】
	// `left`        // 是一个流操纵符 (I/O manipulator)，来自 `<iomanip>` 头文件。它设置后续输出在字段宽度内左对齐。
	// `setw(N)`     // 也是一个流操纵符，设置下一个输出项的字段宽度为 N 个字符。如果实际内容不足N个字符，会用填充字符补足；如果超出，通常会完整显示而破坏对齐。
	// `right`       // 流操纵符，设置后续输出右对齐。
	// `"日期"`, `"描述"`, `"类别"`, `"金额\n"` // 是要打印的表头文本。`\n` 用于在金额后换行。
	cout << left                                  // 设置后续输出为左对齐。
			  << setw(12) << "日期"                 // "日期"字段宽度为12。
			  << setw(30) << "描述"                 // "描述"字段宽度为30。
			  << setw(20) << "类别"                 // "类别"字段宽度为20。
			  << right << setw(10) << "金额\n";     // "金额"字段宽度为10，且右对齐，然后换行。
	// `string(total_width, '-')` // 创建一个包含 `total_width` 个连字符 `'-'` 的字符串。
	                               // `12 + 30 + 20 + 10` 是表头各列宽度的总和。
	cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 打印一行由连字符组成的分隔线。

	// 【循环遍历并打印每一条开销记录】
	// `for (int i = 0; i < expenseCount; ++i)` // 一个标准的 `for` 循环，用于遍历 `allExpenses` 数组中所有有效的记录。
	                                          // `i` 从 0 开始，直到 `expenseCount - 1`。
	for (int i = 0; i < expenseCount; ++i) { // 对每一条记录进行操作：
		// `allExpenses[i]` // 访问数组中索引为 `i` 的 `Expense` 对象。
		// `.getYear()`, `.getMonth()`, `.getDay()`, `.getDescription()`, `.getCategory()`, `.getAmount()` // 调用该对象的各个 getter 方法获取数据。
		cout << left                                                    // 设置当前行输出左对齐。
				  // 日期格式化: YYYY-MM-DD
				  << setw(4) << allExpenses[i].getYear() << "-"          // 年份 (4位宽)，后跟连字符。
				  // `setfill('0')` // 流操纵符，设置当字段宽度大于实际内容时，用字符 '0' 来填充，而不是默认的空格。
				  // 这常用于月份和日期，确保它们总是两位数，例如 "05" 而不是 " 5"。
				  << setfill('0') << setw(2) << allExpenses[i].getMonth() << "-" // 月份 (2位宽, 不足则前面补0)，后跟连字符。
				  << setw(2) << allExpenses[i].getDay()                             // 日期 (2位宽, 不足则前面补0)。
				  // `setfill(' ')` // 将填充字符恢复为默认的空格，以避免影响后续输出。
				  << setfill(' ') << "  "                                        // 恢复填充为空格，并输出两个空格作为日期和描述之间的分隔。
				  // 描述和类别
				  << setw(30) << allExpenses[i].getDescription()             // 描述 (30位宽)。
				  << setw(20) << allExpenses[i].getCategory()                // 类别 (20位宽)。
				  // 金额格式化
				  // `fixed` // 流操纵符，强制浮点数以定点表示法输出 (例如 123.45 而不是科学计数法 1.2345e+2)。
				  // `setprecision(2)` // 流操纵符，设置浮点数输出时的小数精度为2位。
				  << right << fixed << setprecision(2) << setw(10) << allExpenses[i].getAmount() << "\n"; // 金额 (10位宽, 右对齐, 固定2位小数)，然后换行。
	} // `for` 循环结束。
	cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 在列表末尾打印另一行分隔线。
} // `displayAllExpenses` 函数结束。

// 【`displayMonthlySummary` 方法实现 - 显示月度开销统计】
// `void ExpenseTracker::displayMonthlySummary()` // 定义 `ExpenseTracker` 类的 `displayMonthlySummary` 成员方法。
void ExpenseTracker::displayMonthlySummary() {
	int year, month; // 声明两个整型变量 `year` 和 `month`，用于存储用户希望统计的年份和月份。
	cout << "\n--- 月度开销统计 ---\n"; // 打印该功能的标题。

	// 【获取用户输入的年份】
	cout << "输入要统计的年份 (YYYY) (-1 取消): "; // 提示用户输入年份，并告知输入-1可以取消操作。
	// `while (true)` // 开始一个无限循环，用于持续获取和验证用户输入，直到获得有效年份或用户取消。
	while (true) {
		cin >> year; // 尝试从标准输入读取一个整数到 `year` 变量。
		// `cin.fail()` // 检查上一次输入操作是否失败（例如，用户输入了文本而不是数字）。
		if (cin.fail()) { // 如果输入失败
			cout << "年份输入无效，请重新输入 (-1 取消): "; // 打印错误提示。
			cin.clear();        // 清除输入流 `cin` 的错误状态标志，使其恢复正常。
			clearInputBuffer(); // 调用自定义函数清除输入缓冲区中的无效内容（包括换行符）。
		// `else if (year == -1)` // 如果用户输入的是 -1 (表示取消)
		} else if (year == -1) {
			clearInputBuffer(); // 清除输入缓冲区中的 "-1" 后的换行符。
			cout << "已取消月度统计。\n"; // 打印取消消息。
			return; // 从 `displayMonthlySummary` 函数返回，终止统计操作。
		} else { // `else` 分支：如果输入操作成功且不是 -1 (即用户输入了一个数字)
			clearInputBuffer(); // 清除输入缓冲区中数字后的换行符。
			break; // `break;` 跳出当前的 `while (true)` 循环，因为已获得有效的年份输入。
		} // 年份输入验证结束。
	} // 年份获取循环结束。

	// 【获取用户输入的月份】
	cout << "输入要统计的月份 (MM) (-1 取消): "; // 提示用户输入月份。
	// `while (true)` // 开始另一个无限循环，用于获取和验证月份输入。
	while (true) {
		cin >> month; // 尝试读取月份。
		if (cin.fail()) { // 如果输入失败
			cout << "月份输入无效 (1-12)，请重新输入 (-1 取消): "; // 打印错误提示。
			cin.clear();        // 清除错误状态。
			clearInputBuffer(); // 清除无效输入。
		} else if (month == -1) { // 如果用户输入 -1 (取消)
			clearInputBuffer(); // 清除换行符。
			cout << "已取消月度统计。\n"; // 打印取消消息。
			return; // 从函数返回。
		// `else if (month < 1 || month > 12)` // 如果输入的月份不在有效范围 1 到 12 之内
		} else if (month < 1 || month > 12) {
			cout << "月份输入无效 (1-12)，请重新输入 (-1 取消): "; // 打印范围错误提示。
			clearInputBuffer(); // 清除无效月份输入后的换行符，以便下一次循环可以正确读取。
		} else { // `else` 分支：如果输入成功、不是 -1，并且月份在 1-12 范围内 (即月份有效)
			clearInputBuffer(); // 清除有效月份后的换行符。
			break; // `break;` 跳出当前的 `while (true)` 循环。
		} // 月份输入验证结束。
	} // 月份获取循环结束。

	// 【打印月度统计报告的标题】
	// `setfill('0')` // 设置填充字符为 '0'。
	// `setw(2)`      // 设置字段宽度为2。
	// `month`        // 要输出的月份。
	// 这会确保月份总是以两位数显示，例如 "03" 而不是 "3"。
	// `setfill(' ')` // 将填充字符恢复为默认的空格。
	cout << "\n--- " << year << "年" << setfill('0') << setw(2) << month << setfill(' ') << "月 开销统计 ---\n";

	// 【初始化用于统计的变量】
	double totalMonthAmount = 0; // `double` 类型变量，用于累加指定月份的总开销金额，初始化为0。
	bool foundRecords = false;   // `bool` 类型变量，用作标志。如果找到了任何属于该月份的记录，则设为 `true`。初始化为 `false`。

	// `CategorySum categorySums[MAX_UNIQUE_CATEGORIES_PER_MONTH];` // 声明一个 `CategorySum` 类型的对象数组。
	                                                              // `CategorySum` 是一个自定义类，用于存储特定类别的名称和该类别的总金额。
	                                                              // `MAX_UNIQUE_CATEGORIES_PER_MONTH` 是一个常量，定义了最多能独立统计多少个不同类别。
	CategorySum categorySums[MAX_UNIQUE_CATEGORIES_PER_MONTH]; // 用于存储每个类别的汇总金额。
	int uniqueCategoriesCount = 0; // `int` 类型变量，记录当前已统计到的不同开销类别的数量，初始化为0。
	// `double maxCategoryTotal = 0.0;` // 这个变量在原始代码中被声明并有赋值操作，但其值后续未被任何逻辑使用。
	                                  // 对于初学者而言，可以理解为这是一个可能在开发过程中遗留的、当前无实际作用的变量。
	double maxCategoryTotal = 0.0; // (此变量当前未被使用)

	// 【打印该月开销明细的表头】 (与 `displayAllExpenses` 中的表头格式相同)
	cout << left
			  << setw(12) << "日期"
			  << setw(30) << "描述"
			  << setw(20) << "类别"
			  << right << setw(10) << "金额\n";
	cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 打印分隔线。

	// 【遍历所有开销记录，筛选并处理属于指定年月的记录】
	for (int i = 0; i < expenseCount; ++i) { // `for` 循环遍历所有已存储的开销记录。
		// `if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month)` // 检查当前遍历到的记录 `allExpenses[i]` 的年份和月份
		                                                                            // 是否与用户指定的 `year` 和 `month` 相匹配。
		if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month) { // 如果记录属于目标月份
			foundRecords = true; // 将 `foundRecords` 标志设置为 `true`，因为至少找到了一条匹配记录。
			// 【打印该条匹配记录的详细信息】 (格式与 `displayAllExpenses` 中的记录打印一致)
			cout << left
					  << setw(4) << allExpenses[i].getYear() << "-"
					  << setfill('0') << setw(2) << allExpenses[i].getMonth() << "-"
					  << setw(2) << allExpenses[i].getDay() << setfill(' ') << "  "
					  << setw(30) << allExpenses[i].getDescription()
					  << setw(20) << allExpenses[i].getCategory()
					  << right << fixed << setprecision(2) << setw(10) << allExpenses[i].getAmount() << "\n";
			// `totalMonthAmount += allExpenses[i].getAmount();` // 将当前记录的金额累加到 `totalMonthAmount` 中。
			totalMonthAmount += allExpenses[i].getAmount(); // 累加到本月总金额。

			// 【按类别汇总金额逻辑】
			bool categoryExists = false; // `bool` 标志，用于判断当前记录的类别是否已经在 `categorySums` 数组中存在了。
			// `for (int j = 0; j < uniqueCategoriesCount; ++j)` // 遍历当前已经统计到的所有独立类别（存储在 `categorySums` 数组的前 `uniqueCategoriesCount` 个元素中）。
			for (int j = 0; j < uniqueCategoriesCount; ++j) {
				// `if (categorySums[j].name == allExpenses[i].getCategory())` // 比较 `categorySums` 数组中第 `j` 个类别的名称
				                                                            // 与当前开销记录 `allExpenses[i]` 的类别名称是否相同。
				if (categorySums[j].name == allExpenses[i].getCategory()) { // 如果找到了已存在的类别
					categorySums[j].total += allExpenses[i].getAmount(); // 将当前记录的金额累加到该类别的总金额 `categorySums[j].total` 上。
					categoryExists = true; // 设置标志，表示类别已存在。
					if (categorySums[j].total > maxCategoryTotal) maxCategoryTotal = categorySums[j].total; // 更新 `maxCategoryTotal` (虽然它未被使用)
					break; // 找到了对应的类别并处理完毕，跳出内层 `for` 循环 (遍历 `categorySums` 的循环)。
				} // 类别匹配判断结束。
			} // 内层 `for` 循环 (遍历已有类别汇总) 结束。

			// `if (!categoryExists && uniqueCategoriesCount < MAX_UNIQUE_CATEGORIES_PER_MONTH)` // 如果 `categoryExists` 为 `false` (即当前记录的类别是一个新的类别)
			                                                                                // 并且 `uniqueCategoriesCount` 小于 `MAX_UNIQUE_CATEGORIES_PER_MONTH` (即还有空间存储新的类别汇总)
			if (!categoryExists && uniqueCategoriesCount < MAX_UNIQUE_CATEGORIES_PER_MONTH) { // 如果是新类别且有空间
				categorySums[uniqueCategoriesCount].name = allExpenses[i].getCategory();  // 将新类别的名称存入 `categorySums` 数组的下一个可用位置。
				categorySums[uniqueCategoriesCount].total = allExpenses[i].getAmount(); // 将当前记录的金额作为该新类别的初始总金额。
				if (categorySums[uniqueCategoriesCount].total > maxCategoryTotal) maxCategoryTotal = categorySums[uniqueCategoriesCount].total; // 更新 `maxCategoryTotal`。
				uniqueCategoriesCount++; // 已统计的独立类别数量加1。
			} // 新类别处理结束。
		} // 记录匹配判断结束。
	} // 外层 `for` 循环 (遍历所有开销记录) 结束。

	// 【输出统计结果】
	if (!foundRecords) { // 如果 `foundRecords` 标志仍然是 `false` (即遍历完所有记录后，没有找到任何属于指定年月的记录)
		cout << "该月份没有开销记录。\n"; // 打印提示信息。
	} else { // `else` 分支：如果找到了记录 (`foundRecords` 为 `true`)
		cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 打印一条分隔线。
		// 打印本月总计金额
		cout << left << setw(12 + 30 + 20) << "本月总计:" // "本月总计:" 文本左对齐，占据前面三列的宽度。
				  << right << fixed << setprecision(2) << setw(10) << totalMonthAmount << "\n\n"; // 总金额右对齐，固定两位小数，占10位宽。然后输出两个换行符，增加间距。

		// `if (uniqueCategoriesCount > 0)` // 如果统计到了任何类别的汇总数据
		if (uniqueCategoriesCount > 0) {
			cout << "按类别汇总:\n"; // 打印"按类别汇总"的标题。
			cout << left << setw(20) << "类别" << right << setw(10) << "总金额\n"; // 打印类别汇总的表头。
			cout << string(30, '-') << "\n"; // 打印分隔线 (30个字符宽)。
			// `for (int i = 0; i < uniqueCategoriesCount; ++i)` // 循环遍历 `categorySums` 数组中所有已统计的类别。
			for (int i = 0; i < uniqueCategoriesCount; ++i) {
				// 打印每个类别的名称和对应的总金额。
				cout << left << setw(20) << categorySums[i].name // 类别名称 (左对齐，20位宽)。
						  << right << fixed << setprecision(2) << setw(10) << categorySums[i].total << "\n"; // 该类别总金额 (右对齐，固定两位小数，10位宽)，然后换行。
			} // 类别汇总打印循环结束。
			cout << string(30, '-') << "\n"; // 打印末尾分隔线。
		} // 类别汇总打印结束。
	} // 统计结果输出结束。
} // `displayMonthlySummary` 函数结束。

// 【`listExpensesByPeriod` 方法实现 - 按指定期间列出开销】
// `void ExpenseTracker::listExpensesByPeriod()` // 定义 `ExpenseTracker` 类的 `listExpensesByPeriod` 成员方法。
                                            // 此方法提供一个子菜单，允许用户按年、月或日来查看开销记录。
void ExpenseTracker::listExpensesByPeriod() {
	int choice; // 声明一个整型变量 `choice`，用于存储用户在子菜单中的选择。
	// `do...while` // 开始一个 `do-while` 循环，用于显示子菜单并处理用户选择，直到用户选择返回主菜单。
	do {
		// 【显示子菜单选项】
		cout << "\n--- 按期间列出开销 --- \n"; // 子菜单标题。
		cout << "1. 按年份列出\n";        // 选项1：按年份。
		cout << "2. 按月份列出\n";        // 选项2：按月份。
		cout << "3. 按日期列出\n";        // 选项3：按日期。
		cout << "0. 返回主菜单\n";      // 选项0：返回。
		cout << "--------------------\n"; // 分隔线。
		cout << "请输入选项: ";        // 提示输入。

		cin >> choice; // 读取用户在子菜单中的选择。
		// 【输入验证】 (与主菜单中的验证逻辑类似)
		if (cin.fail()) { // 如果输入失败
			cin.clear();        // 清除错误状态。
			clearInputBuffer(); // 清除无效输入。
			choice = -1;        // 将 `choice` 设为一个无效值 (例如-1)，确保 `switch` 进入 `default` 或不匹配任何 `case`，以便循环可以继续提示用户。
		} else { // 如果输入成功 (是数字)
			clearInputBuffer(); // 清除数字后的换行符。
		} // 输入验证结束。

		// `switch (choice)` // 根据用户的子菜单选择执行相应操作。
		switch (choice) {
			case 1: { // 【用户选择1：按年份列出】
				cout << "\n--- 按年份列出开销 ---\n"; // 功能标题。
				cout << "输入年份 (YYYY) (输入 0 返回): "; // 提示输入年份，0可返回子菜单。
				int year; // 声明用于存储年份的变量。
				// `while (!(cin >> year))` // 循环，直到用户输入一个有效的整数年份。
				                         // `!(cin >> year)` 如果读取失败（例如输入文本），则为 `true`，循环继续。
				while (!(cin >> year)) {
					cout << "年份输入无效，请重新输入 (输入 0 返回): "; // 错误提示。
					cin.clear();        // 清除错误。
					clearInputBuffer(); // 清除缓冲区。
				} // 年份输入循环结束。
				clearInputBuffer(); // 清除有效年份后的换行符。
				if (year == 0) break; // 如果用户输入0，则 `break` 跳出当前的 `case 1`，返回到子菜单的 `do-while` 循环开头。

				// `cout << "正在为 " << year << " 年列出开销... (待实现)\n";` // 这行是原始代码中的注释掉的或者是一个占位符提示。
				                                                            // 实际上，下面的代码实现了按年列出的功能。
				bool found = false; // `bool` 标志，用于记录是否找到了该年份的任何开销记录。
				// 【打印表头】 (与 `displayAllExpenses` 中的表头格式相同)
				cout << left
					  << setw(12) << "日期"
					  << setw(30) << "描述"
					  << setw(20) << "类别"
					  << right << setw(10) << "金额\n";
				cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 分隔线。
				// 【遍历所有开销记录，筛选并打印属于指定年份的记录】
				for (int i = 0; i < expenseCount; ++i) { // 遍历所有记录。
					if (allExpenses[i].getYear() == year) { // 如果当前记录的年份与用户指定的年份匹配
						// 【打印该条匹配记录的详细信息】 (格式与 `displayAllExpenses` 中的记录打印一致)
						cout << left
							  << setw(4) << allExpenses[i].getYear() << "-"
							  << setfill('0') << setw(2) << allExpenses[i].getMonth() << "-"
							  << setw(2) << allExpenses[i].getDay() << setfill(' ') << "  "
							  << setw(30) << allExpenses[i].getDescription()
							  << setw(20) << allExpenses[i].getCategory()
							  << right << fixed << setprecision(2) << setw(10) << allExpenses[i].getAmount() << "\n";
						found = true; // 设置 `found` 标志为 `true`，表示已找到记录。
					} // 年份匹配判断结束。
				} // 记录遍历循环结束。
				if (!found) { // 如果遍历完所有记录后，`found` 标志仍为 `false`
					cout << "在 " << year << " 年没有找到开销记录。\n"; // 打印未找到记录的消息。
				} // 未找到记录判断结束。
				cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 打印末尾分隔线。
				break; // 结束 `case 1` 的处理。
			} // `case 1` 结束。

			case 2: { // 【用户选择2：按月份列出】
				cout << "\n--- 按月份列出开销 ---\n"; // 功能标题。
				cout << "输入年份 (YYYY) (输入 0 返回): "; // 提示输入年份。
				int year; // 存储年份。
				while (!(cin >> year)) { /* ... 此处省略了与 case 1 中完全相同的年份输入和验证逻辑的重复注释 ... */
					cout << "年份输入无效，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (year == 0) break; // 年份为0则返回子菜单。

				cout << "输入月份 (MM) (输入 0 返回): "; // 提示输入月份。
				int month; // 存储月份。
				// `while (!(cin >> month) || (month != 0 && (month < 1 || month > 12)))` // 循环，直到输入有效的整数月份 (1-12) 或 0 (返回)。
				                                                                        // `!(cin >> month)`: 读取失败。
				                                                                        // `month != 0 && (month < 1 || month > 12)`: 读取成功但不是0，且月份不在1-12范围内。
				while (!(cin >> month) || (month != 0 && (month < 1 || month > 12))) {
					cout << "月份输入无效 (1-12)，请重新输入 (输入 0 返回): "; // 错误提示。
					cin.clear(); clearInputBuffer(); // 清理。
				} // 月份输入循环结束。
				clearInputBuffer(); // 清除有效月份后的换行符。
				if (month == 0) break; // 月份为0则返回子菜单。

				// `cout << "正在为 " << year << " 年 " << month << " 月列出开销... (待实现)\n";` // 同样是可能的旧注释。
				bool found = false; // 记录是否找到匹配项的标志。
				// 【打印表头】
				cout << left << setw(12) << "日期" /* ... 此处省略表头剩余部分的重复注释 ... */ << setw(10) << "金额\n";
				cout << string(12 + 30 + 20 + 10, '-') << "\n";
				// 【遍历所有开销记录，筛选并打印属于指定年和月的记录】
				for (int i = 0; i < expenseCount; ++i) { // 遍历所有记录。
					if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month) { // 如果年份和月份都匹配
						// 【打印该条匹配记录的详细信息】
						cout << left /* ... 此处省略记录格式化与打印的重复注释 ... */ << "\n";
						found = true; // 设置找到标志。
					} // 年月匹配判断结束。
				} // 记录遍历循环结束。
				if (!found) { // 如果未找到记录
					cout << "在 " << year << " 年 " << month << " 月没有找到开销记录。\n"; // 打印提示。
				} // 未找到判断结束。
				cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 末尾分隔线。
				break; // 结束 `case 2` 的处理。
			} // `case 2` 结束。

			case 3: { // 【用户选择3：按日期列出】
				cout << "\n--- 按日期列出开销 ---\n"; // 功能标题。
				cout << "输入年份 (YYYY) (输入 0 返回): "; // 提示输入年。
				int year; // 存储年。
				while (!(cin >> year)) { /* ... 年份输入与验证逻辑 (同上) ... */
					cout << "年份输入无效，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (year == 0) break;

				cout << "输入月份 (MM) (输入 0 返回): "; // 提示输入月。
				int month; // 存储月。
				while (!(cin >> month) || (month != 0 && (month < 1 || month > 12))) { /* ... 月份输入与验证逻辑 (同上) ... */
					cout << "月份输入无效 (1-12)，请重新输入 (输入 0 返回): ";
					cin.clear(); clearInputBuffer();
				}
				clearInputBuffer();
				if (month == 0) break;

				cout << "输入日期 (DD) (输入 0 返回): "; // 提示输入日。
				int day; // 存储日。
				// `while (!(cin >> day) || (day != 0 && (day < 1 || day > 31)))` // 循环，直到输入有效的整数日期 (1-31) 或 0 (返回)。
				                                                                 // 这是一个基础的日期范围检查，未考虑月份的实际天数。
				while (!(cin >> day) || (day != 0 && (day < 1 || day > 31))) {
					cout << "日期输入无效 (1-31)，请重新输入 (输入 0 返回): "; // 错误提示。
					cin.clear(); clearInputBuffer(); // 清理。
				} // 日期输入循环结束。
				clearInputBuffer(); // 清除有效日期后的换行符。
				if (day == 0) break; // 日期为0则返回子菜单。
				
				// `cout << "正在为 " << year << " 年 " << month << " 月 " << day << " 日列出开销... (待实现)\n";` // 旧注释。
				bool found = false; // 记录是否找到的标志。
				// 【打印表头】
				cout << left << setw(12) << "日期" /* ... 表头 ... */ << setw(10) << "金额\n";
				cout << string(12 + 30 + 20 + 10, '-') << "\n";
				// 【遍历所有开销记录，筛选并打印属于指定年、月、日的记录】
				for (int i = 0; i < expenseCount; ++i) { // 遍历所有记录。
					if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month && allExpenses[i].getDay() == day) { // 如果年、月、日都匹配
						// 【打印该条匹配记录的详细信息】
						cout << left /* ... 记录格式化与打印 ... */ << "\n";
						found = true; // 设置找到标志。
					} // 年月日匹配判断结束。
				} // 记录遍历循环结束。
				if (!found) { // 如果未找到
					cout << "在 " << year << " 年 " << month << " 月 " << day << " 日没有找到开销记录。\n"; // 打印提示。
				} // 未找到判断结束。
				cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 末尾分隔线。
				break; // 结束 `case 3` 的处理。
			} // `case 3` 结束。

			case 0: // 【用户选择0：返回主菜单】
				cout << "返回主菜单...\n"; // 打印返回消息。
				break; // 跳出 `switch` 语句。因为 `choice` 为 0，`do-while` 循环的条件 `choice != 0` 将为 `false`，从而结束子菜单循环。
			default: // 【其他无效输入】
				cout << "无效选项，请重试。\n"; // 打印错误提示。
				// 无需 `break`，因为是 `switch` 的最后一个分支。
		} // `switch` 语句结束。
	} while (choice != 0); // 子菜单的 `do-while` 循环条件：当 `choice` 不为 0 时继续循环。用户选择0则退出。
} // `listExpensesByPeriod` 函数结束。

// 【`saveExpenses` 方法实现 - 保存开销数据到文件】
// `void ExpenseTracker::saveExpenses()` // 定义 `ExpenseTracker` 类的 `saveExpenses` 成员方法。
void ExpenseTracker::saveExpenses() {
	// `ofstream` // 是输出文件流 (output file stream) 类，来自 `<fstream>` 头文件，用于向文件写入数据。
	// `outFile`  // 声明一个 `ofstream` 类型的对象 `outFile`。
	// `(DATA_FILE)`// 在创建 `outFile` 对象时，尝试打开名为 `DATA_FILE` (在程序前面定义的常量，值为 "expenses.dat") 的文件用于写入。
	               // 如果文件不存在，此操作会创建该文件。如果文件已存在，默认情况下，它会清空文件原有内容（覆盖写入）。
	ofstream outFile(DATA_FILE); // 创建并打开用于写入数据的文件流。
	// `if (!outFile)` // 检查文件流对象 `outFile` 是否处于有效状态。如果文件未能成功打开（例如由于权限问题或路径无效），则 `!outFile` 为 `true`。
	if (!outFile) { // 如果文件打开失败
		// `cerr` // 是标准错误输出流，通常也连接到控制台屏幕，专门用于输出错误信息。
		cerr << "错误：无法打开文件 " << DATA_FILE << " 进行写入！\n"; // 向错误流打印错误消息。
		return; // 从 `saveExpenses` 函数返回，不执行后续的保存操作。
	} // 文件打开检查结束。

	// 【将数据写入文件】
	// `outFile << expenseCount << "\n";` // 首先，将当前总的开销记录数 `expenseCount` 写入文件，
	                                   // 并在其后写入一个换行符 `\n`，以便在加载时可以先读取这个数量。
	outFile << expenseCount << "\n"; // 写入记录总数。
	// `for (int i = 0; i < expenseCount; ++i)` // 循环遍历所有有效的开销记录。
	for (int i = 0; i < expenseCount; ++i) {
		// 将每条开销记录的各个字段（通过getter方法获取）依次写入到文件流 `outFile` 中。
		// 字段之间用逗号 `,` 作为分隔符，这是一种简单的CSV (Comma-Separated Values，逗号分隔值) 格式。
		// 每条记录的所有字段写完后，写入一个换行符 `\n`，表示该条记录结束，下一条记录将从新的一行开始。
		outFile << allExpenses[i].getYear() << ","          // 写入年份，后跟逗号。
				<< allExpenses[i].getMonth() << ","         // 写入月份，后跟逗号。
				<< allExpenses[i].getDay() << ","           // 写入日期，后跟逗号。
				<< allExpenses[i].getDescription() << "," // 写入描述，后跟逗号。描述本身可能包含空格，但因为我们按行读取且用逗号分隔，通常能正确处理。
				<< allExpenses[i].getAmount() << ","      // 写入金额，后跟逗号。
				<< allExpenses[i].getCategory() << "\n";    // 写入类别，后跟换行符。
	} // 记录写入循环结束。
	// `outFile.close();` // 关闭文件流。这是一个良好的编程习惯，它确保所有缓冲在内存中的数据都被实际写入到物理文件中，
	                   // 并且释放与该文件关联的系统资源。
	                   // (虽然 `ofstream` 对象在销毁时其析构函数通常会自动关闭文件，但显式调用 `close()` 更明确和安全。)
	outFile.close(); // 关闭文件。
} // `saveExpenses` 函数结束。

// 【`loadExpenses` 方法实现 - 从文件加载开销数据】
// `bool ExpenseTracker::loadExpenses()` // 定义 `ExpenseTracker` 类的 `loadExpenses` 成员方法。
                                     // 此方法尝试从数据文件加载开销记录，并返回一个 `bool` 值：
                                     // `true` 表示加载过程尝试进行（即使可能部分记录无效），`false` 表示文件无法打开或头部信息无效。
bool ExpenseTracker::loadExpenses() {
	// `ifstream` // 是输入文件流 (input file stream) 类，来自 `<fstream>` 头文件，用于从文件读取数据。
	// `inFile`   // 声明一个 `ifstream` 类型的对象 `inFile`。
	// `(DATA_FILE)`// 在创建 `inFile` 对象时，尝试打开名为 `DATA_FILE` 的文件用于读取。
	ifstream inFile(DATA_FILE); // 创建并打开用于读取数据的文件流。
	// `if (!inFile)` // 检查文件流对象 `inFile` 是否有效。如果文件未能成功打开（例如文件不存在或无读取权限），则 `!inFile` 为 `true`。
	if (!inFile) { // 如果文件打开失败
		return false; // 返回 `false`，表示加载失败，程序将开始新的记录。
	} // 文件打开检查结束。

	// 【从文件读取数据内容】
	int countFromFile; // 声明一个整型变量，用于存储从文件第一行读取到的记录总数。
	inFile >> countFromFile; // 尝试从文件流 `inFile` 中读取一个整数，并存入 `countFromFile`。
	                       // 这对应于 `saveExpenses` 中首先写入记录总数的做法。
	// `inFile.fail()` // 检查上一次读取操作（即 `inFile >> countFromFile`）是否失败。
	                 // 失败的可能原因包括：文件为空，文件第一行不是有效的数字，或者达到了文件末尾等。
	// `countFromFile < 0 || countFromFile > MAX_EXPENSES` // 检查读取到的数量是否在一个合理的范围内。
	                                                      // 小于0显然是无效的。大于 `MAX_EXPENSES` 表示文件中的记录数超出了程序内部数组的容量。
	if (inFile.fail() || countFromFile < 0 || countFromFile > MAX_EXPENSES) { // 如果读取数量失败，或数量无效
		expenseCount = 0; // 将程序内部的当前开销记录数 `expenseCount` 重置为0。
		inFile.close();   // 关闭已打开的文件。
		return false;     // 返回 `false`，表示加载失败或文件内容不符合预期。
	} // 记录总数验证结束。
	// `inFile.ignore(numeric_limits<streamsize>::max(), '\n');` // 在成功读取记录总数 `countFromFile` 之后，
	                                                          // 文件流的当前读取位置可能在数字之后、换行符之前（如果数字后有空格或制表符），或者正好在换行符上。
	                                                          // 调用 `ignore` 来跳过并丢弃第一行中剩余的所有字符，直到并包括第一个换行符 `\n`。
	                                                          // 这是为了确保后续的 `getline` 函数能从文件的第二行（即第一条实际的开销记录）开始正确读取。
	inFile.ignore(numeric_limits<streamsize>::max(), '\n'); // 忽略第一行（记录总数那一行）末尾的换行符。

	string line;          // 声明一个 `string` 变量 `line`，用于存储从文件中读取的每一整行文本（即一条序列化后的开销记录）。
	int loadedCount = 0;  // 声明一个整型变量 `loadedCount`，用于计数实际成功加载并解析到内存中的开销记录数量，初始化为0。
	// `for (int i = 0; i < countFromFile; ++i)` // 根据从文件头部读取到的 `countFromFile`，尝试循环读取并解析相应数量的记录行。
	for (int i = 0; i < countFromFile; ++i) {
		// `if (!getline(inFile, line))` // 尝试从文件流 `inFile` 中读取一整行（直到换行符）到 `line` 变量中。
		                              // `getline` 如果成功读取一行，返回 `true` (在布尔上下文中)；如果到达文件末尾或发生错误，返回 `false`。
		                              // `!getline(...)` 表示如果读取失败。
		if (!getline(inFile, line)) { // 如果无法再从文件中读取到一行 (例如文件比声明的记录数要短)
			break; // `break;` 跳出当前的 `for` 循环，停止进一步的加载尝试。
		} // 行读取检查结束。

		// 【解析从文件中读取到的每一行数据】
		// `stringstream ss(line);` // 用当前从文件中读取到的行 `line` 创建一个字符串流 `ss`。
		                           // 字符串流使得我们可以方便地从这个行字符串中按分隔符提取各个字段的值。
		stringstream ss(line); // 将行数据放入字符串流以方便解析。
		string segment; // 声明一个 `string` 变量 `segment`，用于临时存储从行字符串流中按逗号分隔出来的每个数据片段。
		// 声明并初始化用于存储解析出的开销数据的局部变量。
		int year = 0, month = 0, day = 0; // 年、月、日，默认为0。
		string description_str;           // 描述，默认为空字符串。
		double amount = 0.0;              // 金额，默认为0.0。
		string category_str;              // 类别，默认为空字符串。

		// 【逐个字段解析】
		// 使用 `getline(ss, segment, ',')` 从字符串流 `ss` 中读取内容到 `segment`，直到遇到逗号 `,` (逗号本身会被消耗掉但不会放入 `segment`)。
		// 如果成功读取到一个片段，则进行后续的类型转换和错误处理。
		// 如果 `getline` 失败（例如行中没有足够的逗号分隔的字段），则该记录解析失败。

		// 解析年份
		if (getline(ss, segment, ',')) { // 尝试读取年份片段。
			// `try...catch` // 错误处理块。`stoi(segment)` 尝试将字符串 `segment` 转换为整数。
			try { year = stoi(segment); } // 转换字符串到整数。
			// `catch (const invalid_argument& ia)` // 如果 `segment` 不是有效的数字字符串 (例如 "abc")，`stoi` 会抛出 `std::invalid_argument` 异常。
			catch (const invalid_argument& ia) { cerr << "警告：无效年份格式 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; /* `continue` 跳过当前 for 循环的剩余部分，直接开始下一次循环（处理下一行）。 */ }
			// `catch (const out_of_range& oor)` // 如果 `segment` 是数字但超出了 `int` 类型能表示的范围，`stoi` 会抛出 `std::out_of_range` 异常。
			catch (const out_of_range& oor) { cerr << "警告：年份超出范围 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; }
		} else { cerr << "警告：记录 " << i+1 << " 数据不完整 (年份)。\n"; continue; } // 如果连年份片段都读不到，说明行数据不完整，跳过此记录。

		// 解析月份 (逻辑与年份解析类似)
		if (getline(ss, segment, ',')) {
			try { month = stoi(segment); } catch (const invalid_argument& ia) { cerr << "警告：无效月份格式 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; } catch (const out_of_range& oor) { cerr << "警告：月份超出范围 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; }
		} else { cerr << "警告：记录 " << i+1 << " 数据不完整 (月份)。\n"; continue; }
		
		// 解析日期 (逻辑与年份解析类似)
		if (getline(ss, segment, ',')) {
			try { day = stoi(segment); } catch (const invalid_argument& ia) { cerr << "警告：无效日期格式 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; } catch (const out_of_range& oor) { cerr << "警告：日期超出范围 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; }
		} else { cerr << "警告：记录 " << i+1 << " 数据不完整 (日期)。\n"; continue; }

		// 解析描述 (描述是字符串，不需要类型转换，但需要检查长度)
		if (getline(ss, description_str, ',')) { // 尝试读取描述片段。
			// `if (description_str.length() > Expense::MAX_DESCRIPTION_LENGTH)` // 如果读取到的描述长度超过了预设的最大长度
			if (description_str.length() > Expense::MAX_DESCRIPTION_LENGTH) {
				description_str = description_str.substr(0, Expense::MAX_DESCRIPTION_LENGTH); // 将描述截断到最大允许长度。
			}
		} else { cerr << "警告：记录 " << i+1 << " 数据不完整 (描述)。\n"; continue; } // 如果描述片段读不到，跳过此记录。
		
		// 解析金额 (使用 `stod` 将字符串转换为 `double` 类型)
		if (getline(ss, segment, ',')) { // 尝试读取金额片段。
			// `stod(segment)` // 尝试将字符串 `segment` 转换为 `double`。
			try { amount = stod(segment); } // 转换。
			// 类似 `stoi`，`stod` 也会在转换失败时抛出 `std::invalid_argument` 或 `std::out_of_range` 异常。
			catch (const invalid_argument& ia) { cerr << "警告：无效金额格式 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; }
			catch (const out_of_range& oor) { cerr << "警告：金额超出范围 '" << segment << "' 在记录 " << i+1 << "。跳过此记录。\n"; continue; }
		} else { cerr << "警告：记录 " << i+1 << " 数据不完整 (金额)。\n"; continue; } // 如果金额片段读不到，跳过。

		// 解析类别 (类别是行中的最后一个字段)
		// `if (getline(ss, category_str))` // 注意这里调用 `getline` 时没有第三个参数（分隔符）。
		                               // 这意味着它会从字符串流 `ss` 的当前位置读取所有剩余的字符，直到流结束（即行尾），并存入 `category_str`。
		if (getline(ss, category_str)) { // 尝试读取类别片段（即行中剩余的部分）。
			// `if (category_str.length() > Expense::MAX_CATEGORY_LENGTH)` // 如果类别长度超限
			if (category_str.length() > Expense::MAX_CATEGORY_LENGTH) {
				category_str = category_str.substr(0, Expense::MAX_CATEGORY_LENGTH); // 截断类别字符串。
			}
		} else { 
			// 如果这里 `getline` 失败，可能意味着金额后面紧跟着就是行结束符，没有类别字段，或者类别字段为空。
			// 在这种情况下，`category_str` 会保持其默认的空字符串状态。
			// 程序允许这种情况，后续 `setData` 时空类别可能会被处理或使用默认值（例如 "未分类"，取决于 `Expense` 类的具体实现，不过当前 `Expense` 类并没有为 `category` 设置默认值）。
		} // 类别解析结束。
		
		// 【将成功解析的数据存入开销对象数组】
		// `if (loadedCount < MAX_EXPENSES)` // 检查当前已成功加载的记录数 `loadedCount` 是否仍小于程序内部数组 `allExpenses` 的最大容量 `MAX_EXPENSES`。
		if (loadedCount < MAX_EXPENSES) { // 如果数组还有空间
		   // `allExpenses[loadedCount]` // 访问数组中下一个可用的 `Expense` 对象。
		   // `.setData(...)` // 调用该对象的 `setData` 方法，将从文件中解析出的各个字段值设置给它。
		   allExpenses[loadedCount].setData(year, month, day, description_str, amount, category_str); // 将解析的数据设置到对象。
		   loadedCount++; // 成功加载并存储的记录数加1。
		} else { // 如果 `loadedCount` 已经等于 `MAX_EXPENSES` (数组已满)
			// 即使文件中声明的 `countFromFile` 还有更多记录，但程序内部已无法存储，所以停止加载。
			break; // 跳出 `for` 循环。
		} // 容量检查结束。
	} // `for` 循环（遍历文件中的记录行）结束。
	// `expenseCount = loadedCount;` // 将 `ExpenseTracker` 对象的成员变量 `expenseCount` (表示当前程序中有效的开销记录总数)
	                             // 更新为实际成功从文件中加载并解析到 `allExpenses` 数组中的记录数量 `loadedCount`。
	expenseCount = loadedCount; // 更新当前程序中的总开销数量。
	inFile.close(); // 关闭输入文件流。
	return true;    // 返回 `true`，表示加载过程已尝试执行完毕（即使可能跳过了某些无效记录）。
} // `loadExpenses` 函数结束。

// 【`readLastSettlement` 方法实现 - 读取上次自动结算的年月】
// `void ExpenseTracker::readLastSettlement(int& lastYear, int& lastMonth)` // 定义 `ExpenseTracker` 类的 `readLastSettlement` 私有成员方法。
                                                                        // 这个方法用于从结算状态文件 (`SETTLEMENT_FILE`) 中读取上一次自动结算完成的年份和月份。
                                                                        // 参数 `lastYear` 和 `lastMonth` 是整型引用 (`int&`)，
                                                                        // 这意味着函数内部对这两个变量的修改会直接影响到调用该函数时传入的原始变量的值。
void ExpenseTracker::readLastSettlement(int& lastYear, int& lastMonth) {
	lastYear = 0;  // 将传入的 `lastYear` 引用参数初始化（或重置）为0。这作为一种默认值，如果文件读取失败或文件不存在，年份将保持为0。
	lastMonth = 0; // 同样，将 `lastMonth` 初始化（或重置）为0。
	// `ifstream inFile(SETTLEMENT_FILE);` // 创建一个输入文件流 `inFile`，并尝试打开名为 `SETTLEMENT_FILE` (常量，通常是 "settlement_status.txt") 的文件进行读取。
	ifstream inFile(SETTLEMENT_FILE); // 打开结算状态记录文件。
	// `if (inFile)` // 这是一个简洁的检查文件是否成功打开的方式。如果文件成功打开，`inFile` 对象在布尔上下文中会评估为 `true`。
	if (inFile) { // 如果文件成功打开
		// `inFile >> lastYear >> lastMonth;` // 从文件流 `inFile` 中尝试读取两个整数，并分别存入 `lastYear` 和 `lastMonth` 变量中。
		                                   // 期望文件中的格式是：年份 数字，然后是一个空格或换行，然后是月份 数字。
		inFile >> lastYear >> lastMonth; // 读取上一次结算的年份和月份。
		inFile.close(); // 关闭文件流。即使不显式调用，当 `inFile` 对象离开作用域时其析构函数也会关闭文件，但显式关闭是好习惯。
	} // 文件读取操作结束。
	// 如果文件未能成功打开（例如文件不存在），则 `if (inFile)` 条件为 `false`，函数体内的读取操作不会执行，
	// `lastYear` 和 `lastMonth` 将保持它们被初始化时的值0。
} // `readLastSettlement` 函数结束。

// 【`writeLastSettlement` 方法实现 - 写入新的自动结算年月】
// `void ExpenseTracker::writeLastSettlement(int year, int month)` // 定义 `ExpenseTracker` 类的 `writeLastSettlement` 私有成员方法。
                                                               // 这个方法用于将指定的 `year` 和 `month` 作为新的结算点写入到结算状态文件 (`SETTLEMENT_FILE`) 中。
                                                               // 这通常在一次自动结算完成后被调用，以记录当前结算到的进度。
void ExpenseTracker::writeLastSettlement(int year, int month) {
	// `ofstream outFile(SETTLEMENT_FILE);` // 创建一个输出文件流 `outFile`，并尝试打开 `SETTLEMENT_FILE` 进行写入。
	                                      // 打开文件进行写入时，如果文件已存在，其原有内容通常会被清空（覆盖模式）。
	ofstream outFile(SETTLEMENT_FILE); // 打开结算状态记录文件以进行写入。
	// `if (outFile)` // 检查文件是否成功打开以供写入。
	if (outFile) { // 如果文件成功打开
		// `outFile << year << " " << month << endl;` // 将传入的 `year`、一个空格字符、传入的 `month`，以及一个换行符 (`endl`) 依次写入文件流。
		                                            // `endl` (end line) 不仅会写入换行符，通常还会刷新输出缓冲区，确保数据立即写入文件。
		outFile << year << " " << month << endl; // 将年份和月份写入文件，用空格隔开，并以换行结束。
		outFile.close(); // 关闭文件流。
	} else { // `else` 分支：如果文件未能成功打开以供写入
		// `cerr` // 标准错误输出流。
		cerr << "错误：无法写入结算状态文件 " << SETTLEMENT_FILE << "\n"; // 向标准错误输出打印一条错误消息。
	} // 文件写入操作结束。
} // `writeLastSettlement` 函数结束。

// 【`generateMonthlyReportForSettlement` 方法实现 - 为自动结算生成月度报告】
// `void ExpenseTracker::generateMonthlyReportForSettlement(int year, int month)` // 定义 `ExpenseTracker` 类的 `generateMonthlyReportForSettlement` 私有成员方法。
                                                                             // 这个函数的功能与 `displayMonthlySummary` 非常相似，都是生成指定年月的开销报告。
                                                                             // 主要区别在于：
                                                                             // 1. 它是私有的，主要由 `performAutomaticSettlement` 内部调用。
                                                                             // 2. 输出的报告标题会明确指出这是"自动结算"生成的报告。
                                                                             // 3. 它不直接从用户获取年月，而是通过参数传入。
void ExpenseTracker::generateMonthlyReportForSettlement(int year, int month) {
	// 打印报告标题，包含指定的年份和月份，并注明是"(自动结算)"
	cout << "\n--- " << year << "年" << setfill('0') << setw(2) << month << setfill(' ') << "月 开销报告 (自动结算) ---\n";

	// 【初始化统计变量】(与 `displayMonthlySummary` 中的逻辑相同)
	double totalMonthAmount = 0; // 用于累加该月总开销。
	bool foundRecords = false;   // 标志是否找到该月的记录。

	CategorySum categorySums[MAX_UNIQUE_CATEGORIES_PER_MONTH]; // 存储按类别汇总的金额。
	int uniqueCategoriesCount = 0; // 已统计的独立类别数量。
	// `double maxCategoryTotal = 0.0;` // 此变量在 `displayMonthlySummary` 中也存在且未被使用，这里同样。
	double maxCategoryTotal = 0.0; // (此变量当前未被使用)

	cout << "明细:\n"; // 打印"明细:"子标题。
	// 【打印表头】 (与 `displayMonthlySummary` 和 `displayAllExpenses` 中的表头格式一致)
	cout << left
			  << setw(12) << "日期"
			  << setw(30) << "描述"
			  << setw(20) << "类别"
			  << right << setw(10) << "金额\n";
	cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 分隔线。

	// 【遍历所有开销记录，筛选、打印并汇总属于指定年月的记录】
	// 这部分的逻辑与 `displayMonthlySummary` 方法中处理记录的部分完全相同。
	// 因此，关于这部分循环、条件判断、打印格式化、金额累加、类别汇总的详细注释，请参考 `displayMonthlySummary` 方法中的对应注释。
	for (int i = 0; i < expenseCount; ++i) {
		if (allExpenses[i].getYear() == year && allExpenses[i].getMonth() == month) {
			foundRecords = true;
			cout << left
					  << setw(4) << allExpenses[i].getYear() << "-"
					  << setfill('0') << setw(2) << allExpenses[i].getMonth() << "-"
					  << setw(2) << allExpenses[i].getDay() << setfill(' ') << "  "
					  << setw(30) << allExpenses[i].getDescription()
					  << setw(20) << allExpenses[i].getCategory()
					  << right << fixed << setprecision(2) << setw(10) << allExpenses[i].getAmount() << "\n";
			totalMonthAmount += allExpenses[i].getAmount();

			bool categoryExists = false;
			for (int j = 0; j < uniqueCategoriesCount; ++j) {
				if (categorySums[j].name == allExpenses[i].getCategory()) {
					categorySums[j].total += allExpenses[i].getAmount();
					categoryExists = true;
					// 更新 maxCategoryTotal (虽然未使用)
					if (categorySums[j].total > maxCategoryTotal) maxCategoryTotal = categorySums[j].total;
					break;
				}
			}
			if (!categoryExists && uniqueCategoriesCount < MAX_UNIQUE_CATEGORIES_PER_MONTH) {
				categorySums[uniqueCategoriesCount].name = allExpenses[i].getCategory();
				categorySums[uniqueCategoriesCount].total = allExpenses[i].getAmount();
				// 更新 maxCategoryTotal (虽然未使用)
				if (categorySums[uniqueCategoriesCount].total > maxCategoryTotal) maxCategoryTotal = categorySums[uniqueCategoriesCount].total;
				uniqueCategoriesCount++;
			}
		}
	}

	// 【输出统计结果】
	if (!foundRecords) { // 如果该月份没有任何记录
		cout << "该月份没有开销记录。\n"; // 打印提示。
		return; // 直接从函数返回，不再输出后续的汇总信息。
	} // 未找到记录判断结束。

	// 打印分隔线和本月总计 (逻辑与 `displayMonthlySummary` 相同)
	cout << string(12 + 30 + 20 + 10, '-') << "\n";
	cout << left << setw(12 + 30 + 20) << "本月总计:"
			  << right << fixed << setprecision(2) << setw(10) << totalMonthAmount << "\n\n";

	// 打印按类别汇总 (逻辑与 `displayMonthlySummary` 相同)
	if (uniqueCategoriesCount > 0) {
		cout << "按类别汇总:\n";
		cout << left << setw(20) << "类别" << right << setw(10) << "总金额\n";
		cout << string(30, '-') << "\n";
		for (int i = 0; i < uniqueCategoriesCount; ++i) {
			cout << left << setw(20) << categorySums[i].name
					  << right << fixed << setprecision(2) << setw(10) << categorySums[i].total << "\n";
		}
		cout << string(30, '-') << "\n";
	} // 类别汇总打印结束。

	cout << "--- 报告生成完毕 ---\n"; // 打印报告生成结束的标志。
} // `generateMonthlyReportForSettlement` 函数结束。

// 【`performAutomaticSettlement` 方法实现 - 执行自动结算】
// `void ExpenseTracker::performAutomaticSettlement()` // 定义 `ExpenseTracker` 类的 `performAutomaticSettlement` 公有成员方法。
                                                     // 此方法在程序启动时（在构造函数中）被调用，用于自动检查并处理（生成报告）
                                                     // 从上一次结算点到当前月份之前的所有未结算月份的开销数据。
void ExpenseTracker::performAutomaticSettlement() {
	int lastSettledYear, lastSettledMonth; // 声明变量，用于存储从结算文件中读取到的上一次结算的年份和月份。
	readLastSettlement(lastSettledYear, lastSettledMonth); // 调用 `readLastSettlement` 方法，尝试读取上次的结算信息，结果会存入 `lastSettledYear` 和 `lastSettledMonth`。

	// 【获取当前系统的年份和月份】
	time_t now = time(0);                 // 获取当前时间的 `time_t` 表示。
	tm *ltm = localtime(&now);            // 将 `time_t` 转换为 `tm` 结构体以获取本地时间。
	int currentYear = 1900 + ltm->tm_year; // 当前公元年份。
	int currentMonth = 1 + ltm->tm_mon;    // 当前月份 (1-12)。

	// 【处理首次运行或无结算记录的情况】
	// 如果 `readLastSettlement` 后 `lastSettledYear` 仍然是0 (其初始值)，
	// 这通常意味着程序是首次运行，或者结算状态文件 (`SETTLEMENT_FILE`) 不存在或内容无效。
	if (lastSettledYear == 0) { // 如果是首次运行或无有效结算记录
		// 在这种情况下，需要设定一个"基准"的上次结算点。
		// 这里的逻辑是将"上次结算点"设定为"当前月份的前一个月"。
		// 这样做的目的是，当程序下次（或在后续的迭代中）执行自动结算时，它会从这个"上个月"开始检查，
		// 而不是试图从一个非常遥远的过去（比如公元0年）开始检查，这避免了不必要的循环和处理。
		lastSettledYear = currentYear;  // 先将上次结算年月设为当前年月。
		lastSettledMonth = currentMonth;
		if (lastSettledMonth == 1) { // 如果当前是1月份
			lastSettledMonth = 12;   // 那么"上个月"就是去年的12月。
			lastSettledYear--;       // 年份减1。
		} else { // 如果当前不是1月份
			lastSettledMonth--;      // "上个月"就是当前月份减1。
		} // 上个月计算完毕。
		// 将这个计算出来的"基准上次结算点"写入到结算状态文件中。
		writeLastSettlement(lastSettledYear, lastSettledMonth);
		// 向用户显示一条消息，告知已设置的基准结算点。
		cout << "首次运行或无结算记录，已设置基准结算点为: "
				  << lastSettledYear << "年" << setfill('0') << setw(2) << lastSettledMonth << setfill(' ') << "月。\n";
		return; // 设置完基准点后，本次 `performAutomaticSettlement` 调用即结束，不再执行后续的追溯结算逻辑。
	} // 首次运行处理结束。

	// 【从上次记录的结算点开始，逐月检查并处理，直到当前月份之前】
	// 初始化要开始检查结算的年份和月份，使用从文件中读取到的 `lastSettledYear` 和 `lastSettledMonth`。
	int yearToSettle = lastSettledYear;
	int monthToSettle = lastSettledMonth;

	// `while (true)` // 开始一个循环，这个循环会不断地将 `yearToSettle` 和 `monthToSettle` 向前推进一个月，
	               // 并为每个推进后的月份生成报告，直到达到不需要再结算的条件。
	while (true) {
		// 首先，将要结算的月份向前推进一个月。
		monthToSettle++; // 月份加1。
		if (monthToSettle > 12) { // 如果月份超过12 (例如从12月变成13月)
			monthToSettle = 1;    // 月份重置为1月。
			yearToSettle++;       // 年份加1。
		} // 月份和年份推进完毕。

		// 【判断是否已追溯到不需要再结算的月份】
		// 自动结算的目标是处理"过去"未结算的月份。
		// 因此，如果 `yearToSettle` 已经超过了 `currentYear` (当前年份)，
		// 或者，如果 `yearToSettle` 等于 `currentYear` 但 `monthToSettle` 已经等于或超过了 `currentMonth` (当前月份)，
		// 这都意味着我们已经追溯到了当前月份或更远的未来，此时应该停止自动结算。
		if (yearToSettle > currentYear || (yearToSettle == currentYear && monthToSettle >= currentMonth)) {
			break; // `break;` 跳出当前的 `while (true)` 循环，自动结算过程结束。
		} // 停止条件判断结束。

		// 【为当前需要结算的 `yearToSettle` 和 `monthToSettle` 生成报告】
		// 打印开始结算当前月份的提示信息。
		cout << "\n>>> 开始自动结算: " << yearToSettle << "年" << setfill('0') << setw(2) << monthToSettle << setfill(' ') << "月 <<\n";
		// 调用 `generateMonthlyReportForSettlement` 方法为这个指定的年月生成并显示月度开销报告。
		generateMonthlyReportForSettlement(yearToSettle, monthToSettle);
		// 调用 `writeLastSettlement` 方法，将当前已成功结算的 `yearToSettle` 和 `monthToSettle` 写入结算状态文件，
		// 这样下次程序启动时，就会从这个新的结算点开始。
		writeLastSettlement(yearToSettle, monthToSettle);
		// 打印完成结算当前月份的提示信息。
		cout << ">>> 自动结算完成: " << yearToSettle << "年" << setfill('0') << setw(2) << monthToSettle << setfill(' ') << "月 <<\n";
	} // `while (true)` 循环结束（当所有需要自动结算的月份都处理完毕时）。
} // `performAutomaticSettlement` 函数结束。

// 【`deleteExpense` 方法实现 - 删除指定的开销记录】
// `void ExpenseTracker::deleteExpense()` // 定义 `ExpenseTracker` 类的 `deleteExpense` 公有成员方法。
                                      // 此方法允许用户查看所有开销记录，并选择一条进行删除。
void ExpenseTracker::deleteExpense() {
	if (expenseCount == 0) { // 首先检查当前是否有任何开销记录。
		cout << "没有开销记录可供删除。\n"; // 如果没有记录，打印提示消息。
		return; // 并从函数返回，不执行后续的删除逻辑。
	} // 记录存在性检查结束。

	cout << "\n--- 删除开销记录 ---\n"; // 打印功能标题。
	cout << "以下是所有开销记录:\n"; // 提示信息。
	// 【显示所有开销记录及其序号，方便用户选择要删除的记录】
	// 表头增加了一列 "序号"。
	cout << left                                           // 设置后续输出左对齐。
			  << setw(5) << "序号"                           // "序号"列，宽度为5。
			  << setw(12) << "日期"                          // "日期"列，宽度为12。
			  << setw(30) << "描述"                          // "描述"列，宽度为30。
			  << setw(20) << "类别"                          // "类别"列，宽度为20。
			  << right << setw(10) << "金额\n";              // "金额"列，宽度为10，右对齐，然后换行。
	cout << string(5 + 12 + 30 + 20 + 10, '-') << "\n"; // 打印分隔线，宽度为所有列宽之和。

	// `for (int i = 0; i < expenseCount; ++i)` // 循环遍历所有有效的开销记录。
	for (int i = 0; i < expenseCount; ++i) {
		// 打印每条记录的详细信息，并在最前面加上序号。
		// `i + 1` // 因为数组索引 `i` 从0开始，所以用户看到的序号应该是 `i + 1`，从1开始计数，更符合用户习惯。
		cout << left
				  << setw(5) << i + 1 // 打印记录的序号 (1-based)。
				  // 后续的日期、描述、类别、金额的打印格式与 `displayAllExpenses` 方法中的相同。
				  << setw(4) << allExpenses[i].getYear() << "-"
				  << setfill('0') << setw(2) << allExpenses[i].getMonth() << "-"
				  << setw(2) << allExpenses[i].getDay() << setfill(' ') << "  "
				  << setw(30) << allExpenses[i].getDescription()
				  << setw(20) << allExpenses[i].getCategory()
				  << right << fixed << setprecision(2) << setw(10) << allExpenses[i].getAmount() << "\n";
	} // 记录列表打印循环结束。
	cout << string(5 + 12 + 30 + 20 + 10, '-') << "\n"; // 打印列表末尾的分隔线。

	// 【获取用户要删除的记录序号】
	int recordNumberToDelete; // 声明一个整型变量，用于存储用户输入的要删除的记录的序号。
	cout << "请输入要删除的记录序号 (0 取消删除): "; // 提示用户输入序号，并告知输入0可以取消删除。
	// `while (!(cin >> recordNumberToDelete) || recordNumberToDelete < 0 || recordNumberToDelete > expenseCount)` // 开始一个循环，用于获取和验证用户输入的序号。
	                                                                                                           // 循环条件解释：
	                                                                                                           // `!(cin >> recordNumberToDelete)`: 如果从 `cin` 读取整数到 `recordNumberToDelete` 失败 (例如用户输入了文本)，则为 `true`。
	                                                                                                           // `recordNumberToDelete < 0`: 如果用户输入的序号小于0 (无效)。
	                                                                                                           // `recordNumberToDelete > expenseCount`: 如果用户输入的序号大于当前总记录数 (无效，因为序号是从1到`expenseCount`)。
	                                                                                                           // 只要以上任何一个条件为 `true` (通过 `||` 或运算符连接)，整个循环条件就为 `true`，循环继续，提示用户重新输入。
	while (!(cin >> recordNumberToDelete) || recordNumberToDelete < 0 || recordNumberToDelete > expenseCount) {
		cout << "输入无效。请输入 1 到 " << expenseCount << " 之间的数字，或 0 取消: "; // 打印错误提示，明确告知有效的输入范围。
		cin.clear();        // 清除 `cin` 的错误状态。
		clearInputBuffer(); // 清除输入缓冲区中的无效内容。
	} // 序号输入和验证循环结束。
	clearInputBuffer(); // 清除用户输入有效序号后，残留在缓冲区中的换行符。

	if (recordNumberToDelete == 0) { // 如果用户输入的是0 (表示取消删除操作)
		cout << "取消删除操作。\n"; // 打印取消消息。
		return; // 从 `deleteExpense` 函数返回，不执行任何删除。
	} // 取消判断结束。

	// `int indexToDelete = recordNumberToDelete - 1;` // 将用户输入的基于1的记录序号 `recordNumberToDelete` 转换为基于0的数组索引 `indexToDelete`。
	                                                // 例如，如果用户想删除第1条记录，其数组索引是0。
	int indexToDelete = recordNumberToDelete - 1; // 计算要删除的记录在数组中的实际索引。

	// 【显示即将被删除的记录的详细信息，让用户进行第一次确认】
	cout << "\n即将删除以下记录:\n"; // 提示信息。
	// 打印表头 (不带序号列)
	cout << left
			  << setw(12) << "日期"
			  << setw(30) << "描述"
			  << setw(20) << "类别"
			  << right << setw(10) << "金额\n";
	cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 分隔线。
	// 打印指定索引 `indexToDelete` 的那条开销记录的详细信息。
	cout << left
			  << setw(4) << allExpenses[indexToDelete].getYear() << "-"
			  << setfill('0') << setw(2) << allExpenses[indexToDelete].getMonth() << "-"
			  << setw(2) << allExpenses[indexToDelete].getDay() << setfill(' ') << "  "
			  << setw(30) << allExpenses[indexToDelete].getDescription()
			  << setw(20) << allExpenses[indexToDelete].getCategory()
			  << right << fixed << setprecision(2) << setw(10) << allExpenses[indexToDelete].getAmount() << "\n";
	cout << string(12 + 30 + 20 + 10, '-') << "\n"; // 末尾分隔线。

	// 【第一次确认删除】
	char confirm; // 声明一个字符变量 `confirm`，用于存储用户的确认输入 (y/n)。
	cout << "确认删除吗？ (y/n): "; // 提示用户输入确认。
	cin >> confirm; // 读取用户输入的单个字符到 `confirm`。
	clearInputBuffer(); // 清除输入字符后的换行符。

	// `if (confirm == 'y' || confirm == 'Y')` // 检查用户是否输入了 'y' 或 'Y' (表示同意删除)。
	if (confirm == 'y' || confirm == 'Y') { // 如果用户第一次确认删除
		// 【进行第二次最终确认 - 为了防止用户误操作，这是一个重要的安全措施】
		cout << "\n警告：此操作无法撤销！\n"; // 打印警告信息。
		cout << "最后一次确认，真的要删除这条记录吗？ (y/n): "; // 再次提示用户确认。
		char final_confirm; // 声明字符变量 `final_confirm` 用于存储用户的最终确认。
		cin >> final_confirm; // 读取最终确认输入。
		clearInputBuffer(); // 清除换行符。

		// `if (final_confirm == 'y' || final_confirm == 'Y')` // 检查用户是否再次输入了 'y' 或 'Y'。
		if (final_confirm == 'y' || final_confirm == 'Y') { // 如果用户最终确认删除
			cout << "\n正在删除记录...\n"; // 打印正在删除的消息。

			// 【执行删除操作：通过将后续元素前移来覆盖被删除的元素】
			// 这个 `for` 循环从要被删除的元素的索引 `indexToDelete` 开始，
			// 一直遍历到数组中倒数第二个有效元素的索引 (`expenseCount - 1` 是最后一个有效元素的索引，所以 `expenseCount - 2` 是倒数第二个，循环条件 `i < expenseCount - 1` 意味着 `i` 的最大值是 `expenseCount - 2`)。
			// 在循环体中，`allExpenses[i] = allExpenses[i + 1];` 将第 `i+1` 个元素的值赋给第 `i` 个元素。
			// 这样做的效果是，从被删除位置开始，后面的所有元素都向前移动了一个位置，从而覆盖掉了原来在 `indexToDelete` 位置的元素。
			for (int i = indexToDelete; i < expenseCount - 1; ++i) {
				allExpenses[i] = allExpenses[i + 1]; // 将后一个元素的数据复制到前一个元素的位置。
			} // 元素前移循环结束。
			// `expenseCount--;` // 将总的开销记录数 `expenseCount` 减1，因为已经删除了一条记录。
			expenseCount--; // 更新记录总数。
			cout << "记录已删除。\n"; // 打印删除成功的消息。
			saveExpenses(); // 调用 `saveExpenses()` 方法，将删除操作后的数据（即更新后的 `allExpenses` 数组和 `expenseCount`）立即保存到文件中。
			cout << "数据已自动保存。\n"; // 提示数据已保存。
		} else { // `else` 分支：如果用户在最终确认时没有输入 'y' 或 'Y' (即取消了删除)
			cout << "已取消删除操作（二次确认未通过）。\n"; // 打印取消消息。
		} // 最终确认结束。
	} else { // `else` 分支：如果用户在第一次确认时没有输入 'y' 或 'Y' (即取消了删除)
		cout << "取消删除操作。\n"; // 打印取消消息。
	} // 第一次确认结束。
} // `deleteExpense` 函数结束。

// --- Main Function ---
// 【`main` 函数 - C++程序的入口点】
// `int main()` // `main` 是每个C++程序执行开始的地方。操作系统会调用这个函数来启动程序。
              // `int` 表示 `main` 函数在执行完毕后会向操作系统返回一个整数状态码。
              // 通常，返回0表示程序成功执行并正常结束，非0值表示程序遇到了某种错误或异常结束。
int main() {
	// `ExpenseTracker tracker;` // 创建一个 `ExpenseTracker` 类的对象（实例），并将其命名为 `tracker`。
	                          // 当这行代码执行时，`ExpenseTracker` 类的构造函数 (`ExpenseTracker::ExpenseTracker()`) 会被自动调用。
	                          // 构造函数会进行一些初始化工作，比如将 `expenseCount` 初始化为0，尝试从文件加载已有的开销数据 (`loadExpenses()`)，
	                          // 以及执行首次的自动结算检查 (`performAutomaticSettlement()`)。
	ExpenseTracker tracker; // 创建开销追踪器对象。
	// `tracker.run();` // 调用 `tracker` 对象的 `run()` 成员方法。
	                 // `run()` 方法包含了程序的主要用户交互循环（显示菜单、获取用户选择、调用相应功能等）。
	                 // 程序的大部分时间都会在这个 `run()` 方法中执行，直到用户选择退出程序 (在 `run` 方法中对应选项6)。
	tracker.run();         // 运行开销追踪器的主程序逻辑。
	// 当 `tracker.run()` 方法结束执行（即用户选择了退出选项）后，程序流程会回到这里。
	// `return 0;` // `main` 函数返回0给操作系统，表示程序已成功执行完毕。
	return 0; // 程序正常结束。
} // `main` 函数结束。
// 文件的末尾


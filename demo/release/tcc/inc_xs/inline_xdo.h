


// 数据库字段数据类型
#define XDO_DT_UNKNOWN		-1
#define XDO_DT_NULL			0
#define XDO_DT_INT			1
#define XDO_DT_NUM			2
#define XDO_DT_DATE			3
#define XDO_DT_TIME			4
#define XDO_DT_DATETIME		5
#define XDO_DT_TEXT			6
#define XDO_DT_BINARY		7



// 数据库连接对象
typedef struct XDO_Driver_Struct XDO_Driver_Struct, *XDO_Driver;
typedef struct XDO_Connect_Struct {
	const char* Host;							// 数据库服务器地址 或 数据库连接串
	int Port;									// 数据库连接端口
	const char* User;							// 数据库认证账号
	const char* Pwd;							// 数据库认证密码
	const char* DataBase;						// 默认数据库
	const char* Charset;						// 默认编码
	XDO_Driver objDriver;						// 数据库驱动对象指针
	void* objDB;								// 数据库连接对象
} XDO_Connect_Struct, *XDO_Connect;



// 记录集对象
typedef struct XDO_RecordsetStruct {
	XDO_Connect objConn;						// 数据库连接对象指针
	XDO_Driver objDriver;						// 数据库驱动对象指针
} XDO_RecordsetStruct, *XDO_Recordset;



// 数据库驱动
typedef struct XDO_Driver_Struct {
	
	// 连接数据库
	int (*Connect)(XDO_Connect objConn);
	
	// 断开数据库连接
	int (*Disconnect)(XDO_Connect objConn);
	
	// 执行 SQL 语句（无返回值，例如增、删、改）
	int (*Execute)(XDO_Connect objConn, char* sSQL);
	
	// 执行 SQL 语句，返回记录集
	XDO_Recordset (*Select)(XDO_Connect objConn, char* sSQL);
	
	// 释放记录集
	int (*RS_Free)(void* objRS);
	
	// 获取记录集包含的字段数量
	int (*RS_GetFieldCount)(void* objRS);
	
	// 获取记录集包含的记录数量
	int (*RS_GetRecordCount)(void* objRS);
	
	// 从记录集中获取字段名，字段序号从 0 开始
	char* (*RS_GetFieldName)(void* objRS, int idx);
	
	// 从记录集中获取字段数据类型，字段序号从 0 开始
	int (*RS_GetFieldType)(void* objRS, int idx);
	
	// 判断记录集中指定字段是否为主键，字段序号从 0 开始
	int (*RS_FieldIsPrimaryKey)(void* objRS, int idx);
	
	// 判断记录集中指定字段是否不能为空，字段序号从 0 开始
	int (*RS_FieldIsNotNull)(void* objRS, int idx);
	
	// 记录集检索下一条记录
	int (*RS_Next)(void* objRS);
	
	// 记录集获取当前记录某一列的值，列号从 0 开始
	char* (*RS_GetValue)(void* objRS, int idx);

	// 增删改查接口
    long (*Insert)(XDO_Connect objConn, const char* table, const char* columns, const char* values);
    int (*Update)(XDO_Connect objConn, const char* table, const char* set_clause, const char* where);
    int (*Delete)(XDO_Connect objConn, const char* table, const char* where);
    int (*BeginTransaction)(XDO_Connect objConn);
    int (*Commit)(XDO_Connect objConn);
    int (*Rollback)(XDO_Connect objConn);
	
} XDO_Driver_Struct;



// 创建连接对象
XDO_Connect xdoCreate(XDO_Driver objDriver);

// 连接到数据库
int xdoConnect(XDO_Connect objConn);

// 关闭数据库连接
int xdoDisconnect(XDO_Connect objConn);

// 销毁连接对象
int xdoDestroy(XDO_Connect objConn);

// 执行 SQL 语句（无返回值）
int xdoExecute(XDO_Connect objConn, char* sSQL);

// 执行 SQL 语句（有返回值）
XDO_Recordset xdoSelect(XDO_Connect objConn, char* sSQL);

// 插入记录
long xdoInsert(XDO_Connect objConn, const char* table, const char* columns, const char* values);

// 更新记录
int xdoUpdate(XDO_Connect objConn, const char* table, const char* set_clause, const char* where);

// 删除记录
int xdoDelete(XDO_Connect objConn, const char* table, const char* where);

// 准备事务
int xdoBeginTransaction(XDO_Connect objConn);

// 提交事务
int xdoCommit(XDO_Connect objConn);

// 回滚事务
int xdoRollback(XDO_Connect objConn);

// 释放记录集（DB_Select返回值）
int xrsFree(XDO_Recordset objRS);

// 获取记录集包含的字段数量
int xrsGetFieldCount(XDO_Recordset objRS);

// 获取记录集包含的记录数量
int xrsGetRecordCount(XDO_Recordset objRS);

// 从记录集中获取字段名，字段序号从 0 开始
char* xrsGetFieldName(XDO_Recordset objRS, int idx);

// 从记录集中获取字段数据类型，字段序号从 0 开始
int xrsGetFieldType(XDO_Recordset objRS, int idx);

// 判断记录集中指定字段是否为主键，字段序号从 0 开始
int xrsFieldIsPrimaryKey(XDO_Recordset objRS, int idx);

// 判断记录集中指定字段是否不能为空，字段序号从 0 开始
int xrsFieldIsNotNull(XDO_Recordset objRS, int idx);

// 记录集检索下一条记录
int xrsNext(XDO_Recordset objRS);

// 记录集获取当前记录某一列的值，列号从 0 开始
char* xrsGetValue(XDO_Recordset objRS, int idx);



// 连接到 SQLite3 数据库
XDO_Connect xdoConnectSQLite(char* sFile);



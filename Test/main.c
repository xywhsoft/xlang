// 用于解析C源的语法示例，
// 由 Terence Parr 改编自 Java 等效示例
// 作者：Jim Idle - 2007年4月
// 只要引用了所有原始作者，就可以以任何方式使用此示例代码。

// set ts=4,sw=4
// Tab 长度 4 字符, 缩进 长度 4 字符

// 注意：尽管提供的所有示例都配置为由Visual Studio 2005构建，但基于
// $(ANTLRSRC)/code/antlr/main/runtime/C/vs2005/rulefiles/antlr3.rules
// 中提供的自定义构建规则，没有理由必须如此。
// 如果您知道如何运行antlr工具，那么只需使用gcc或任何方式将生成的.c文件和此文件一起编译：
// gcc *.c -I. -o XXX
// C代码是通用的，将在所有平台上编译和运行（请向antlr兴趣新闻组报告任何警告或错误（请参见www.antlr.org）
// 以便可以针对我没有专门测试过的任何平台进行更正。
// 项目设置（如附加库路径和包含路径）是相对于ANTLR性能系统上源代码所在的位置设置的。
// 您可能需要更改设置以查找includes和lib文件。
// UNIX用户需要 -L path/to/antlr/libs -lantlr3c（发布模式）或 -lantlr3cd（调试）

// 您可以通过各种方式采用自己的做法，但通常最好为项目创建一个include
// 其中包括ANTLR3C运行时头文件、生成的头文件（所有这些都可以安全地多次包含）和您自己的项目相关头文件。
// 使用<>在编译行中包含和-I（vs2005现在处理，而vs2003没有处理）。



#include    "CLexer.h"
#include    "CParser.h"



// 本例的主要切入点
int ANTLR3_CDECL main(int argc, char *argv[])
{
	// 现在我们声明我们需要的ANTLR相关的局部变量。
	// 注意，除非您确信您的项目永远不需要线程安全版本，否则您应该始终为每次调用创建实例变量。
	
	// 输入文件的名称。
	// 请注意，对于ASCII/8位字符串，我们总是使用抽象类型pANTLR3_UINT8
	// 运行库保证这在所有平台上都很好。
	// 这是一个通用规则-始终使用ANTLR3提供的typedef作为指针/类型等。
    pANTLR3_UINT8	    fName;
    
    // ANTLR3字符输入流，它对输入源进行了抽象，以便很容易从不同的源（如文件或内存字符串）提供输入。
    // 对于ASCII/latin-1内存字符串，请使用：
    //     input = antlr3NewAsciiStringInPlaceStream (stringtouse, (ANTLR3_UINT64) length, NULL);
    // 对于UCS2（16位）内存字符串，请使用：
    //     input = antlr3NewUCS2StringInPlaceStream (stringtouse, (ANTLR3_UINT64) length, NULL);
    // 有关文件的输入，请参见下面的代码
    // 注意，这本质上是指向包含指向函数的指针的结构的指针。
    // 您可以创建自己的输入流类型（复制现有类型之一），并在创建标准版本后通过安装自己的指针来覆盖任何单个函数。
    pANTLR3_INPUT_STREAM    input;
    
    // 当然，lexer 是由ANTLR生成的，因此 lexer 类型不是大写。
    // lexer 提供了一个 pANTLR3_INPUT_STREAM，它从中消耗输入并生成一个令牌流作为输出。
    pCLexer		    lxr;
    
    // 令牌流由ANTLR3生成的lexer生成。
    // 同样，它是一个基于结构的 API/Object，您可以根据需要自定义和重写方法。
    // 一个令牌流被提供给生成的解析器，如果您愿意，您可以编写自己的令牌流并将其传入。
    pANTLR3_COMMON_TOKEN_STREAM	    tstream;
    
    // C解析器也由ANTLR生成，并接受如上所述的令牌流。
    // 事实上，令牌流可以是任何源，只要它实现ANTLR3_token_source接口即可。
    // 在这种情况下，解析器不返回任何内容，但它当然可以从调用时调用的规则中指定任何类型的返回类型。
    pCParser				psr;
    
    // 根据在命令行上提供给我们的参数创建输入流。
    // 在本例中，如果没有显式参数，输入将始终默认为 ./input
    if ( argc < 2 || argv[1] == NULL ) {
		fName	=(pANTLR3_UINT8)"./input"; // 注意，在VS2005调试中，必须配置工作目录
    } else {
		fName	= (pANTLR3_UINT8)argv[1];
    }
    
    // 使用提供的文件名创建输入流
    input	= antlr3FileStreamNew(fName, ANTLR3_ENC_8BIT);
    
    // 如果有足够的内存和文件存在等，将成功创建输入
    if ( input == NULL ) {
	    fprintf(stderr, "Failed to open file %s\n", (char *)fName);
		exit(1);
	}
	
	// 我们的输入流现在是打开的，所有设置都已就绪，因此我们可以创建lexer的新实例，并将lexer输入设置为输入流：
	// (file | memory | ?) --> inputstream -> lexer --> tokenstream --> parser ( --> treeparser )?
    lxr	    = CLexerNew(input);	    // CLexerNew 是 ANTLR 生成的函数

    // 需要检查错误
    if ( lxr == NULL ) {
	    fprintf(stderr, "内存申请失败，无法创建 lexer\n");
	    exit(1);
    }
    
    // 我们的lexer已经就位，因此我们可以从中创建令牌流
    // 注意：除了文件被读取之外，还没有发生任何事情。
    // 我们只是将所有这些东西连接在一起，当我们调用解析器规则时将调用它们。
    // ANTLR3_SIZE_HINT 通常可以保留为默认值，除非您有非常大的令牌流/输入。
    // 每个生成的lexer都提供一个令牌源接口，这是令牌流创建器的第二个参数。
    // 请注意，即使您实现了自己的令牌结构，它也始终包含一个标准的公共令牌，这是您传递给其他所有对象的指针。
    // 一个公共令牌，作为一个指针，它应该指向您自己的外部令牌结构。
	tstream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lxr));
	
    if ( tstream == NULL ) {
		fprintf(stderr, "尝试分配令牌流时内存不足\n");
		exit(1);
    }
	
    // 最后，现在我们已经构造了 lexer，我们可以创建解析器（Parser）
    psr	    = CParserNew(tstream);  // CParserNew 是 ANTLR 生成的函数
	
    if ( psr == NULL ) {
		fprintf(stderr, "尝试分配解析器时内存不足\n");
		exit(ANTLR3_ERR_NOMEM);
    }
    
    // 我们都准备好出发了。虽然乍一看很复杂，
    // 事实上，上面的大部分代码都是处理错误的，实际上没有那么多事情要做（C中不是总是这样吗？；-）
    // 因此，我们现在调用解析器。
    // ANTLR3生成的C组件的所有元素以及ANTLRC运行库本身都是伪对象。
    // 这意味着它们被表示为指向结构的指针，其中包含它们需要的任何实例数据，以及指向其他接口或“方法”的一组指针。
    // 请注意，一般来说，我们在这里创建的这几个指针是唯一可以显式 free() 的东西，因为其他所有东西都是通过工厂创建的，
    // 它们可以高效地分配内存，并在关闭 parser/lexer 等时自动 free() 它们使用的所有东西。
    // 注意，这仅意味着方法总是通过对象指针调用，任何方法的第一个参数都是指向结构本身的指针。
    // 它还有一个副作用，如果您使用的是VS2005这样的IDE，那么当您键入->时，您将看到该对象支持的方法列表。
    
    psr->translation_unit(psr);
    
    // 我们没有从这个解析器规则返回任何内容，所以我们可以完成。
    // 它只剩下关闭我们打开的对象，按照我们创建它们的相反顺序
    psr	    ->free  (psr);	    psr = NULL;
    tstream ->free  (tstream);	    tstream = NULL;
    lxr	    ->free  (lxr);	    lxr = NULL;
    input   ->close (input);	    input = NULL;
	
    return 0;
}


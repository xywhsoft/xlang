/* Included by the existing type-model translation unit. Both frontend and
 * Typed XLIR supply views; the control-flow semantics live only here. */
#include "src/type/xl6_constructor_flow.h"

enum { CF_NORMAL, CF_RETURN, CF_THROW, CF_BREAK, CF_CONTINUE, CF_EXITS };
enum { CF_PENDING=1, CF_READY=2 };
typedef struct XL6_ConstructorPaths { unsigned exits[CF_EXITS]; } XL6_ConstructorPaths;
typedef struct XL6_ConstructorSolver {
    const XL6_ConstructorView* view;
    size_t visits;
    unsigned depth;
    int ok;
} XL6_ConstructorSolver;

static XL6_ConstructorNode cfChild(XL6_ConstructorSolver* s,XL6_ConstructorNode n,size_t i)
{ return n&&i<s->view->count(s->view->context,n)?s->view->child(s->view->context,n,i):0; }
static XL6_AstKind cfKind(XL6_ConstructorSolver* s,XL6_ConstructorNode n)
{ return n?s->view->kind(s->view->context,n):XL6_AST_LIST; }
static int cfText(XL6_ConstructorSolver* s,XL6_ConstructorNode n,const char* text)
{ const char* p=n?s->view->text(s->view->context,n):NULL;return p&&strcmp(p,text)==0; }
static void cfError(XL6_ConstructorSolver* s,XL6_ConstructorNode n,const char* text)
{ if(s->ok&&s->view->error)s->view->error(s->view->context,n,text);s->ok=0; }
static void cfJoin(XL6_ConstructorPaths* a,XL6_ConstructorPaths b)
{ for(unsigned i=0;i<CF_EXITS;++i)a->exits[i]|=b.exits[i]; }
static int cfEnter(XL6_ConstructorSolver* s,XL6_ConstructorNode n)
{
    if(!s->ok)return 0;
    if(++s->visits>s->view->maxVisits||s->depth>=256){
        cfError(s,n,"constructor definite-assignment analysis limit exceeded");return 0;
    }
    ++s->depth;return 1;
}
static int cfTruth(XL6_ConstructorSolver* s,XL6_ConstructorNode n)
{
    unsigned negate=0;
    for(unsigned depth=0;depth<256;++depth){
        if(cfKind(s,n)==XL6_AST_BOOL)return (cfText(s,n,"true")?1:0)^(int)negate;
        if(cfKind(s,n)!=XL6_AST_UNARY||(!cfText(s,n,"not")&&!cfText(s,n,"!")))return -1;
        negate^=1;n=cfChild(s,n,0);
    }
    cfError(s,n,"constructor condition analysis depth exceeded");
    return -1;
}
static int cfCaptures(XL6_ConstructorSolver* s,XL6_ConstructorNode n)
{
    if(!n||!cfEnter(s,n))return 0;
    unsigned f=s->view->facts(s->view->context,n);int found=0;
    if(f&(XL6_CONSTRUCTOR_SELF|XL6_CONSTRUCTOR_DIRECT_FIELD|XL6_CONSTRUCTOR_BASE_CALL))found=1;
    for(size_t i=0;!found&&i<s->view->count(s->view->context,n);++i)
        found=cfCaptures(s,cfChild(s,n,i));
    --s->depth;return found;
}
static XL6_ConstructorPaths cfNode(XL6_ConstructorSolver*,XL6_ConstructorNode,unsigned);
static void cfSequence(XL6_ConstructorSolver* s,XL6_ConstructorPaths* p,XL6_ConstructorNode n)
{
    unsigned entry=p->exits[CF_NORMAL];p->exits[CF_NORMAL]=0;
    cfJoin(p,cfNode(s,n,entry));
}
static XL6_ConstructorPaths cfExpression(XL6_ConstructorSolver* s,XL6_ConstructorNode n,unsigned entry)
{
    XL6_ConstructorPaths p={{entry,0,0,0,0}};
    XL6_AstKind k=cfKind(s,n);unsigned f=s->view->facts(s->view->context,n);
    if(k==XL6_AST_ANON_FUNC||k==XL6_AST_FUNC||k==XL6_AST_ASYNC_FUNC||k==XL6_AST_EXPORT_FUNC){
        if((entry&CF_PENDING)&&cfCaptures(s,n))
            cfError(s,n,"incomplete self cannot be captured by a closure");
        if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=entry;
        return p;
    }
    if(f&XL6_CONSTRUCTOR_DIRECT_FIELD){
        if((f&XL6_CONSTRUCTOR_FIELD)&&(entry&CF_PENDING))
            cfError(s,n,"native resource field is read before constructor definite assignment");
        if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=entry;
        return p; /* Only the resolved self receiver is a construction borrow. */
    }
    if((f&XL6_CONSTRUCTOR_SELF)&&(entry&CF_PENDING))
        cfError(s,n,"incomplete self cannot escape or be passed to user code");
    if(k==XL6_AST_MULTI_ASSIGN&&s->view->count(s->view->context,n)==2){
        /* Product RHS is evaluated once, before sequential checked writes. */
        cfSequence(s,&p,cfChild(s,n,1));
        XL6_ConstructorNode targets=cfChild(s,n,0);
        for(size_t i=0;targets&&i<s->view->count(s->view->context,targets);++i){
            XL6_ConstructorNode lhs=cfChild(s,targets,i);unsigned lf=s->view->facts(s->view->context,lhs);
            if(!(lf&XL6_CONSTRUCTOR_DIRECT_FIELD))cfSequence(s,&p,lhs);
            if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=p.exits[CF_NORMAL];
            if((lf&XL6_CONSTRUCTOR_FIELD)&&p.exits[CF_NORMAL])p.exits[CF_NORMAL]=CF_READY;
        }
        return p;
    }
    if(k==XL6_AST_ASSIGN&&s->view->count(s->view->context,n)==2){
        XL6_ConstructorNode lhs=cfChild(s,n,0);unsigned lf=s->view->facts(s->view->context,lhs);
        if(!(lf&XL6_CONSTRUCTOR_DIRECT_FIELD)||!cfText(s,n,"="))cfSequence(s,&p,lhs);
        cfSequence(s,&p,cfChild(s,n,1));
        /* A failed insertion does not initialize a slot. In particular, catch
         * must see the state before this write, even after RHS succeeded. */
        if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=p.exits[CF_NORMAL];
        if((lf&XL6_CONSTRUCTOR_FIELD)&&p.exits[CF_NORMAL])p.exits[CF_NORMAL]=CF_READY;
        return p;
    }
    if(k==XL6_AST_CONDITIONAL&&s->view->count(s->view->context,n)==3){
        cfSequence(s,&p,cfChild(s,n,0));unsigned in=p.exits[CF_NORMAL];p.exits[CF_NORMAL]=0;
        int truth=cfTruth(s,cfChild(s,n,0));
        if(truth!=0)cfJoin(&p,cfNode(s,cfChild(s,n,1),in));
        if(truth!=1)cfJoin(&p,cfNode(s,cfChild(s,n,2),in));
        if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=p.exits[CF_NORMAL];
        return p;
    }
    if(k==XL6_AST_BINARY&&(cfText(s,n,"and")||cfText(s,n,"&&")||cfText(s,n,"or")||cfText(s,n,"||")||cfText(s,n,"??"))){
        cfSequence(s,&p,cfChild(s,n,0));
        unsigned in=p.exits[CF_NORMAL];int t=cfTruth(s,cfChild(s,n,0));
        int isAnd=cfText(s,n,"and")||cfText(s,n,"&&");
        if(t<0||cfText(s,n,"??")){cfJoin(&p,cfNode(s,cfChild(s,n,1),in));}
        else if(t==isAnd){p.exits[CF_NORMAL]=0;cfJoin(&p,cfNode(s,cfChild(s,n,1),in));}
        if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=p.exits[CF_NORMAL];
        return p;
    }
    size_t first=(f&XL6_CONSTRUCTOR_BASE_CALL)?1:0;
    for(size_t i=first;i<s->view->count(s->view->context,n);++i)cfSequence(s,&p,cfChild(s,n,i));
    if((f&XL6_CONSTRUCTOR_BASE_EXPOSES)&&(p.exits[CF_NORMAL]&CF_PENDING))
        cfError(s,n,"base constructor may expose self before derived fields are initialized");
    if(f&XL6_CONSTRUCTOR_MAY_RAISE)p.exits[CF_THROW]|=p.exits[CF_NORMAL];
    if((f&XL6_CONSTRUCTOR_BASE_INITIALIZES)&&p.exits[CF_NORMAL])p.exits[CF_NORMAL]=CF_READY;
    return p;
}
static XL6_ConstructorPaths cfIf(XL6_ConstructorSolver* s,XL6_ConstructorNode n,unsigned entry)
{
    XL6_ConstructorPaths p=cfNode(s,cfChild(s,n,0),entry);unsigned in=p.exits[CF_NORMAL];p.exits[CF_NORMAL]=0;
    int truth=cfTruth(s,cfChild(s,n,0));
    if(truth!=0)cfJoin(&p,cfNode(s,cfChild(s,n,1),in));
    if(truth==1)return p;
    XL6_ConstructorNode items=cfChild(s,n,2);
    for(size_t i=0;in&&items&&i<s->view->count(s->view->context,items);++i){
        XL6_ConstructorNode branch=cfChild(s,items,i),cond=cfChild(s,branch,0);
        XL6_ConstructorPaths q=cfNode(s,cond,in);in=q.exits[CF_NORMAL];q.exits[CF_NORMAL]=0;cfJoin(&p,q);
        truth=cfTruth(s,cond);
        if(truth!=0)cfJoin(&p,cfNode(s,cfChild(s,branch,1),in));
        if(truth==1)in=0;
    }
    cfJoin(&p,cfNode(s,cfChild(s,n,3),in));return p;
}
static XL6_ConstructorPaths cfTry(XL6_ConstructorSolver* s,XL6_ConstructorNode n,unsigned entry)
{
    XL6_ConstructorPaths p=cfNode(s,cfChild(s,n,0),entry);
    unsigned thrown=p.exits[CF_THROW];p.exits[CF_THROW]=0;
    XL6_ConstructorNode catches=cfChild(s,n,1);
    for(size_t i=0;thrown&&catches&&i<s->view->count(s->view->context,catches);++i){
        XL6_ConstructorNode c=cfChild(s,catches,i);size_t count=s->view->count(s->view->context,c);
        cfJoin(&p,cfNode(s,cfChild(s,c,count?count-1:0),thrown));
        if(count==2)thrown=0;
    }
    /* Catches' own throws are not caught by siblings. */
    p.exits[CF_THROW]|=thrown;
    XL6_ConstructorNode finallyNode=cfChild(s,n,2);
    if(finallyNode){
        XL6_ConstructorPaths result={{0}};
        for(unsigned exit=0;exit<CF_EXITS;++exit){
            if(!p.exits[exit])continue;
            XL6_ConstructorPaths q=cfNode(s,finallyNode,p.exits[exit]);
            result.exits[exit]|=q.exits[CF_NORMAL];q.exits[CF_NORMAL]=0;cfJoin(&result,q);
        }
        return result;
    }
    return p;
}
static XL6_ConstructorPaths cfLoop(XL6_ConstructorSolver* s,XL6_ConstructorNode n,unsigned entry)
{
    XL6_AstKind k=cfKind(s,n);XL6_ConstructorPaths result={{0}};unsigned head=entry,seen=0;
    int isDo=k==XL6_AST_DO_WHILE,isFor=k==XL6_AST_FOR;
    XL6_ConstructorNode cond=cfChild(s,n,isDo?1:0),body=cfChild(s,n,isFor?2:isDo?0:1);
    if(isFor){
        XL6_ConstructorPaths source=cfNode(s,cfChild(s,n,1),entry);
        head=source.exits[CF_NORMAL];source.exits[CF_NORMAL]=0;cfJoin(&result,source);
        /* Cursor creation, projection and advancement can fail. */
        result.exits[CF_THROW]|=head;
    }
    /* Two bits can enter the loop head, so a least fixed point takes at most
     * two growth steps. Continue executes the condition; break bypasses it. */
    while(head!=seen&&s->ok){
        seen=head;unsigned in=head;XL6_ConstructorPaths q={{0}};
        if(!isDo&&!isFor){
            q=cfNode(s,cond,in);in=q.exits[CF_NORMAL];q.exits[CF_NORMAL]=0;cfJoin(&result,q);
        }
        int truth=isFor?-1:cfTruth(s,cond);
        if(!isDo&&truth!=1)result.exits[CF_NORMAL]|=in;
        if(!isDo&&truth==0)break;
        q=cfNode(s,body,in);result.exits[CF_RETURN]|=q.exits[CF_RETURN];result.exits[CF_THROW]|=q.exits[CF_THROW];
        result.exits[CF_NORMAL]|=q.exits[CF_BREAK];in=q.exits[CF_NORMAL]|q.exits[CF_CONTINUE];
        if(isDo){
            q=cfNode(s,cond,in);in=q.exits[CF_NORMAL];q.exits[CF_NORMAL]=0;cfJoin(&result,q);
            if(truth!=1)result.exits[CF_NORMAL]|=in;
            if(truth==0)break;
        }
        head|=in;
    }
    return result;
}
static XL6_ConstructorPaths cfNode(XL6_ConstructorSolver* s,XL6_ConstructorNode n,unsigned entry)
{
    XL6_ConstructorPaths p={{entry,0,0,0,0}};
    if(!entry||!n||!cfEnter(s,n))return p;
    switch(cfKind(s,n)){
    case XL6_AST_IF:p=cfIf(s,n,entry);break;
    case XL6_AST_WHILE:case XL6_AST_DO_WHILE:case XL6_AST_FOR:p=cfLoop(s,n,entry);break;
    case XL6_AST_TRY:p=cfTry(s,n,entry);break;
    case XL6_AST_RETURN:case XL6_AST_THROW:{
        for(size_t i=0;i<s->view->count(s->view->context,n);++i)cfSequence(s,&p,cfChild(s,n,i));
        unsigned exit=cfKind(s,n)==XL6_AST_RETURN?CF_RETURN:CF_THROW;
        p.exits[exit]|=p.exits[CF_NORMAL];p.exits[CF_NORMAL]=0;break;
    }
    case XL6_AST_BREAK:p.exits[CF_BREAK]=entry;p.exits[CF_NORMAL]=0;break;
    case XL6_AST_CONTINUE:p.exits[CF_CONTINUE]=entry;p.exits[CF_NORMAL]=0;break;
    case XL6_AST_ASSERT:{
        cfSequence(s,&p,cfChild(s,n,0));int truth=cfTruth(s,cfChild(s,n,0));
        if(truth!=1){
            XL6_ConstructorPaths q=cfNode(s,cfChild(s,n,1),p.exits[CF_NORMAL]);
            p.exits[CF_THROW]|=q.exits[CF_NORMAL]|q.exits[CF_THROW];
        }
        if(truth==0)p.exits[CF_NORMAL]=0;
        break;
    }
    case XL6_AST_MATCH:{
        cfSequence(s,&p,cfChild(s,n,0));unsigned in=p.exits[CF_NORMAL];p.exits[CF_NORMAL]=0;
        XL6_ConstructorNode cases=cfChild(s,n,1);
        for(size_t i=0;cases&&i<s->view->count(s->view->context,cases);++i){
            XL6_ConstructorNode c=cfChild(s,cases,i);
            cfJoin(&p,cfNode(s,cfChild(s,c,1),in));
        }
        /* The language requires exhaustive match, checked by both adapters'
         * ordinary semantic/type validation. Unmatched values trap in CFG;
         * they cannot add a normally returning uninitialized path. */
        if(!cases||!s->view->count(s->view->context,cases))
            cfError(s,n,"constructor flow received an empty match");
        break;
    }
    case XL6_AST_TYPE:case XL6_AST_PARAM:case XL6_AST_RETURN_TYPES:break;
    default:p=cfExpression(s,n,entry);break;
    }
    --s->depth;return p;
}
int xl6ConstructorFlowCheck(const XL6_ConstructorView* view,XL6_ConstructorNode body,
    int entryInitialized,int requireNormalExit)
{
    if(!view||!view->kind||!view->count||!view->child||!view->text||!view->facts||!view->maxVisits)return 0;
    XL6_ConstructorSolver s={view,0,0,1};
    XL6_ConstructorPaths p=cfNode(&s,body,entryInitialized?CF_READY:CF_PENDING);
    if(requireNormalExit&&((p.exits[CF_NORMAL]|p.exits[CF_RETURN])&CF_PENDING))
        cfError(&s,body,"constructor normal exit leaves a non-null native resource field uninitialized");
    if(p.exits[CF_BREAK]||p.exits[CF_CONTINUE])cfError(&s,body,"constructor has an unresolved loop exit");
    return s.ok;
}

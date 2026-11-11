#ifndef FIXTURE_XGE3_AUDIO_THREADS_H
#define FIXTURE_XGE3_AUDIO_THREADS_H
#include "foreign_xge3_audio_resources.h"
#include <xge3_audio_resources.h>
#include <__xl_runtime/xl6_foreign_context.h>
#undef NDEBUG
#include <assert.h>
/* Transfer only this helper's own sole view reference. Language references
 * are never consumed behind their owners' backs. */
typedef struct fixture_audio_worker {
    XL6_ForeignView* view;
    uint64_t creator;
    uint64_t owners;
    bool passed;
} fixture_audio_worker;
static int32 fixtureAudioDropWorker(ptr raw)
{
    fixture_audio_worker* state=(fixture_audio_worker*)raw;
    assert(xrtThreadCurrentId()!=state->creator);
    XL6_ForeignViewCallV1 call={0};
    assert(!xl6ForeignViewCallBeginV1(state->view,&call,NULL,0));xrtClearError();
    assert(!xl6ForeignViewCloseV1(state->view));xrtClearError();
    assert(xl6ForeignContextReadyV1());
    xrtownershipscope warm={0};assert(xrtOwnershipMutationBegin(&warm) && xrtOwnershipScopeEnd(&warm));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignViewReleaseV1(state->view));state->view=NULL;
    assert(xl6ForeignViewPendingV1(state->creator)==1);
    assert(xl6ForeignViewDrainV1());
    assert(xl6ForeignViewPendingV1(state->creator)==1);
    assert(fixtureXgeAudioOwners()==state->owners);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    state->passed=true;return 0;
}
static bool fixtureXgeAudioWorkerRelease(int kind,const XL6_CodeOwner* code)
{
    assert(fixtureXgeAudioOwners()==0 && !xl6ForeignViewPendingV1(0));
    void* raw=NULL;const XL6_ForeignViewDescriptorV1* policy=NULL;
    if(kind==0){
        xge_audio_group group=(xge_audio_group)xrtCalloc(1,sizeof(*group));assert(group);
        assert(xgeAudioGroupInit(group)==XGE_OK);raw=group;policy=&xl6Xge3AudioGroupPolicy;
    }else if(kind==1){
        xge_sound sound=NULL;assert(xgeSoundCreateOwned(&sound,fixtureXgeAudioPath(),NULL)==XGE_OK);
        raw=sound;policy=&xl6Xge3SoundPolicy;
    }else if(kind==2){
        xge_music music=NULL;assert(xgeMusicCreateOwned(&music,fixtureXgeAudioPath(),NULL)==XGE_OK);
        raw=music;policy=&xl6Xge3MusicPolicy;
    }else{
        assert(kind==3);xge_stream stream=NULL;assert(xgeStreamCreateOwned(&stream,fixtureXgeAudioPath(),NULL)==XGE_OK);
        raw=stream;policy=&xl6Xge3AudioStreamPolicy;
    }
    fixture_audio_worker state={0};state.creator=xrtThreadCurrentId();state.owners=fixtureXgeAudioOwners();
    state.view=xl6ForeignViewCreateRootV1(raw,policy,code);assert(state.view);
    xthread* thread=xrtThreadCreate(fixtureAudioDropWorker,&state,0);assert(thread);
    assert(xrtThreadWaitFor(thread,30000)==XWAIT_OK && xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
    assert(state.passed && !state.view && xl6ForeignViewPendingV1(state.creator)==1);
    assert(fixtureXgeAudioOwners()==state.owners);
    assert(xl6ForeignViewDrainV1());
    assert(!xl6ForeignViewPendingV1(state.creator) && fixtureXgeAudioOwners()==0);
    return true;
}
#endif

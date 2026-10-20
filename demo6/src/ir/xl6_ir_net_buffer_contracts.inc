#define NB "xl6NetBufferDestroyV1"
#define BF "xl6BufferDestroyV1"
#define FR "xl6NetFrameResultDestroyV1"
#define EF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6NetBufferCreateV1",T,V,1,{I},.resultOwner=NB,.nonblocking=1,EF},
{"xl6NetBufferDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NB},.noRaise=1,.nativeHandlePolicy="xl6NetBufferHandlePolicyV1Get"},
{"xl6NetBufferLengthV1",T,I,1,{V},.parameterOwners={NB},.pure=1,.nonblocking=1,EF},
{"xl6NetBufferSpanCountV1",T,I,1,{V},.parameterOwners={NB},.pure=1,.nonblocking=1,EF},
{"xl6NetBufferLimitV1",T,I,1,{V},.parameterOwners={NB},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetBufferClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NB},.nonblocking=1,EF},
{"xl6NetBufferCompactV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={NB},.nonblocking=1,EF},
{"xl6NetBufferAppendMoveV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={NB,NB},.nonblocking=1,EF},
{"xl6NetBufferConsumeV1",T,I,2,{V,I},.parameterOwners={NB},.nonblocking=1,EF},
{"xl6NetBufferFindV1",T,I,3,{V,I,I},.parameterOwners={NB},.pure=1,.nonblocking=1,EF},
{"xl6NetBufferPeekV1",T,Y,3,{V,I,I},.parameterOwners={NB},.nonblocking=1,EF},
{"xl6NetBufferReadV1",T,Y,2,{V,I},.parameterOwners={NB},.nonblocking=1,EF},
{"xl6NetBufferPeekIntoV1",T,I,4,{V,V,I,I},.parameterOwners={NB,BF},.nonblocking=1,EF},
{"xl6NetBufferReadIntoV1",T,I,3,{V,V,I},.parameterOwners={NB,BF},.nonblocking=1,EF},
#define INPUT(Verb) \
{"xl6NetBuffer" #Verb "TextV1",T,XL6_IR_C_ABI_VOID,3,{V,S,I},.parameterOwners={NB},.nonblocking=1,EF}, \
{"xl6NetBuffer" #Verb "BytesV1",T,XL6_IR_C_ABI_VOID,3,{V,Y,I},.parameterOwners={NB},.nonblocking=1,EF}, \
{"xl6NetBuffer" #Verb "BufferV1",T,XL6_IR_C_ABI_VOID,3,{V,V,I},.parameterOwners={NB,BF},.nonblocking=1,EF},
INPUT(Append)
INPUT(Prepend)
#undef INPUT
{"xl6NetFrameResultDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={FR},.noRaise=1,.nativeHandlePolicy="xl6NetFrameResultHandlePolicyV1Get"},
{"xl6NetFrameResultStatusV1",T,I,1,{V},.parameterOwners={FR},.pure=1,.noRaise=1,.nonblocking=1,.resultEnum=&xl6IrNetFrameStatusContract},
#define FIELD(Name,Abi) {"xl6NetFrameResult" #Name "V1",T,Abi,1,{V},.parameterOwners={FR},.pure=1,.nonblocking=1,EF},
FIELD(PayloadOffset,I)
FIELD(PayloadSize,I)
FIELD(FrameSize,I)
FIELD(Declared,U)
#undef FIELD
{"xl6NetFrameResultBytesV1",T,Y,1,{V},.parameterOwners={FR},.nonblocking=1,EF},
{"xl6NetFrameResultTextV1",T,S,1,{V},.parameterOwners={FR},.nonblocking=1,EF},
{"xl6NetFrameResultCopyIntoV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={FR,BF},.nonblocking=1,EF},
{"xl6NetLineFramerCreateTextV1",T,V,5,{S,I,B,I,I},.resultOwner="xl6NetLineFramerDestroyV1",.nonblocking=1,EF},
{"xl6NetLineFramerCreateBytesV1",T,V,5,{Y,I,B,I,I},.resultOwner="xl6NetLineFramerDestroyV1",.nonblocking=1,EF},
{"xl6NetLengthFramerCreateV1",T,V,7,{I,I,I,I,I,B,I},.resultOwner="xl6NetLengthFramerDestroyV1",.nonblocking=1,EF},
#define FAMILY(Name) \
{"xl6Net" #Name "FramerDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6Net" #Name "FramerHandlePolicyV1Get"}, \
{"xl6Net" #Name "FramerResetV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerBufferedV1",T,I,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.pure=1,.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerLimitV1",T,I,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.pure=1,.noRaise=1,.nonblocking=1}, \
{"xl6Net" #Name "FramerFailedV1",T,B,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.pure=1,.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerNextV1",T,V,1,{V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.resultOwner=FR,.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerCommitV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1",FR},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerNextIntoV1",T,B,3,{V,V,B},.parameterOwners={"xl6Net" #Name "FramerDestroyV1",BF},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerAppendTextV1",T,XL6_IR_C_ABI_VOID,3,{V,S,I},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerAppendBytesV1",T,XL6_IR_C_ABI_VOID,3,{V,Y,I},.parameterOwners={"xl6Net" #Name "FramerDestroyV1"},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerAppendBufferV1",T,XL6_IR_C_ABI_VOID,3,{V,V,I},.parameterOwners={"xl6Net" #Name "FramerDestroyV1",BF},.nonblocking=1,EF}, \
{"xl6Net" #Name "FramerAppendMoveV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={"xl6Net" #Name "FramerDestroyV1",NB},.nonblocking=1,EF},
FAMILY(Line)
FAMILY(Length)
#undef FAMILY
#undef EF
#undef FR
#undef BF
#undef NB

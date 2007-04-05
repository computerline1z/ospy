//
// Copyright (c) 2007 Ole Andr� Vadla Ravn�s <oleavr@gmail.com>
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include "Errors.h"
#include "Marshallers.h"
#include "Signature.h"
#include "Logging.h"

namespace InterceptPP {

using Logging::Logger;

void Initialize();
void UnInitialize();
Logger *GetLogger();
void SetLogger(Logger *logger);

typedef struct {
	DWORD edi;
	DWORD esi;
	DWORD ebp;
	DWORD esp;
	DWORD ebx;
	DWORD edx;
	DWORD ecx;
	DWORD eax;
} CpuContext;

typedef enum {
	CALLING_CONV_UNKNOWN = 0,
	CALLING_CONV_STDCALL,
	CALLING_CONV_THISCALL,
	CALLING_CONV_CDECL,
} CallingConvention;

typedef enum {
    FUNCTION_CALL_ENTERING,
    FUNCTION_CALL_LEAVING,
} FunctionCallState;

typedef enum {
    ARG_DIR_UNKNOWN = 0,
	ARG_DIR_IN      = 1,
	ARG_DIR_OUT     = 2,
} ArgumentDirection;

#pragma pack(push, 1)
typedef struct {
	BYTE CALL_opcode;
	DWORD CALL_offset;
	void *data;
} FunctionTrampoline;

typedef struct {
	BYTE JMP_opcode;
	DWORD JMP_offset;
} FunctionRedirectStub;
#pragma pack(pop)

typedef struct {
	SignatureSpec sig;
	int numBytesToCopy;
} PrologSignatureSpec;

#define FUNCTION_ARGS_SIZE_UNKNOWN -1

class FunctionCall;

typedef bool (*FunctionCallHandler) (FunctionCall *call);

class ArgumentSpec : public BaseObject
{
public:
	ArgumentSpec(const OString &name, ArgumentDirection direction, BaseMarshaller *marshaller)
		: m_name(name), m_direction(direction), m_marshaller(marshaller)
	{
	}
	~ArgumentSpec() { delete m_marshaller; }

    const OString &GetName() const { return m_name; }
    ArgumentDirection GetDirection() const { return m_direction; }
    const BaseMarshaller *GetMarshaller() const { return m_marshaller; }

    unsigned int GetSize() const { return m_marshaller->GetSize(); }

protected:
	OString m_name;
	ArgumentDirection m_direction;
	BaseMarshaller *m_marshaller;
};

class Argument : public BaseObject
{
public:
    Argument(ArgumentSpec *spec, const void *data)
        : m_spec(spec), m_data(data)
    {}

    ArgumentSpec *GetSpec() const { return m_spec; }

    Logging::Node *ToNode(bool deep, IPropertyProvider *propProv) const;
    OString ToString(bool deep, IPropertyProvider *propProv) const;
    bool ToInt(int &result) const;

protected:
    ArgumentSpec *m_spec;
    const void *m_data;
};

class ArgumentListSpec : public BaseObject
{
public:
    ArgumentListSpec();
    ArgumentListSpec(unsigned int count, ...);
    ArgumentListSpec(unsigned int count, va_list args);
	~ArgumentListSpec();

    void AddArgument(ArgumentSpec *arg);

    unsigned int GetSize() const { return m_size; }
    unsigned int GetCount() const { return static_cast<unsigned int>(m_arguments.size()); }
    bool GetHasOutArgs() const { return m_hasOutArgs; }

	ArgumentSpec *operator[](int index) { return m_arguments[index]; }

protected:
    unsigned int m_size;
	OVector<ArgumentSpec *>::Type m_arguments;
    bool m_hasOutArgs;

    void Initialize(unsigned int count, va_list args);
};

class ArgumentList : public BaseObject
{
public:
    ArgumentList(ArgumentListSpec *spec, const void *data);
	~ArgumentList();

    const ArgumentListSpec *GetSpec() const { return m_spec; }

    unsigned int GetCount() const { return static_cast<unsigned int>(m_arguments.size()); }

	const Argument &operator[](int index) const { return m_arguments[index]; }

protected:
    ArgumentListSpec *m_spec;
	OVector<Argument>::Type m_arguments;
};

class FunctionSpec : public BaseObject
{
public:
	FunctionSpec(const OString &name="",
                 CallingConvention conv=CALLING_CONV_UNKNOWN,
                 int argsSize=FUNCTION_ARGS_SIZE_UNKNOWN,
                 FunctionCallHandler handler=NULL)
		: m_name(name),
          m_callingConvention(conv),
		  m_argsSize(argsSize),
          m_argList(NULL),
		  m_handler(handler)
	{}

    ~FunctionSpec()
    {
        if (m_argList)
            delete m_argList;
    }

    void SetParams(const OString &name,
                   CallingConvention conv=CALLING_CONV_UNKNOWN,
                   int argsSize=FUNCTION_ARGS_SIZE_UNKNOWN,
                   FunctionCallHandler handler=NULL)
	{
		SetName(name);
        SetCallingConvention(conv);
		SetArgsSize(argsSize);
        SetHandler(handler);
	}

    ArgumentListSpec *GetArguments() const { return m_argList; }
    void SetArguments(ArgumentListSpec *argList);
    void SetArguments(unsigned int count, ...);

	const OString &GetName() const { return m_name; }
	void SetName(const OString &name) { m_name = name; }

	CallingConvention GetCallingConvention() const { return m_callingConvention; }
	void SetCallingConvention(CallingConvention conv) { m_callingConvention = conv; }

	int GetArgsSize() const { return m_argsSize; }
	void SetArgsSize(int size) { m_argsSize = size; }

	FunctionCallHandler GetHandler() const { return m_handler; }
	void SetHandler(FunctionCallHandler handler) { m_handler = handler; }

protected:
	OString m_name;
	CallingConvention m_callingConvention;
	int m_argsSize;
    ArgumentListSpec *m_argList;
	FunctionCallHandler m_handler;
};

class Function : public BaseObject
{
public:
    Function(FunctionSpec *spec=NULL, DWORD offset=0);

    static void Initialize();
    static void UnInitialize();
    void Initialize(FunctionSpec *spec, DWORD offset) { m_spec = spec; m_offset = offset; }

    virtual const OString GetParentName() const { return ""; }
    OString GetFullName() const;

    FunctionTrampoline *CreateTrampoline(unsigned int bytesToCopy=0);
    FunctionSpec *GetSpec() const { return m_spec; }
    DWORD GetOffset() const { return m_offset; }

    void Hook();
    void UnHook();

protected:
    FunctionSpec *m_spec;
    DWORD m_offset;
    static const PrologSignatureSpec prologSignatureSpecs[];
    static OVector<Signature>::Type prologSignatures;

    void *m_trampoline;
    DWORD m_oldMemProtect;
    LONGLONG m_origStart;

    void OnEnter(FunctionCall *call);
    void OnLeave(FunctionCall *call);

private:
    static void OnEnterProxy(CpuContext cpuCtx, unsigned int unwindSize, FunctionTrampoline *trampoline, void **proxyRet, void **finalRet);
    FunctionTrampoline *OnEnterWrapper(CpuContext *cpuCtx, unsigned int *unwindSize, FunctionTrampoline *trampoline, void *btAddr, DWORD *lastError);

    static void OnLeaveProxy(CpuContext cpuCtx, FunctionTrampoline *trampoline);
    void OnLeaveWrapper(CpuContext *cpuCtx, FunctionTrampoline *trampoline, FunctionCall *call, DWORD *lastError);
};

class FunctionCall : public BaseObject, IPropertyProvider
{
public:
	FunctionCall(Function *function, void *btAddr, CpuContext *cpuCtxEnter);

	Function *GetFunction() const { return m_function; }
	void *GetBacktraceAddress() const { return m_backtraceAddress; }
	void *GetReturnAddress() const { return m_returnAddress; }

    CpuContext *GetCpuContextLive() const { return m_cpuCtxLive; }
	void SetCpuContextLive(CpuContext *cpuCtx) { m_cpuCtxLive = cpuCtx; }

    const CpuContext *GetCpuContextEnter() const { return &m_cpuCtxEnter; }

	const CpuContext *GetCpuContextLeave() const { return &m_cpuCtxLeave; }
    void SetCpuContextLeave(const CpuContext *ctx) { m_cpuCtxLeave = *ctx; }

	DWORD *GetLastErrorLive() const { return m_lastErrorLive; }
	void SetLastErrorLive(DWORD *lastError) { m_lastErrorLive = lastError; }

    const OString &GetArgumentsData() const { return m_argumentsData; }
    const ArgumentList *GetArguments() const { return m_arguments; }

    FunctionCallState GetState() const { return m_state; }
    void SetState(FunctionCallState state) { m_state = state; }

	bool GetShouldCarryOn() const { return m_shouldCarryOn; }
	void SetShouldCarryOn(bool carryOn) { m_shouldCarryOn = carryOn; }

    void *GetUserData() const { return m_userData; }
    void SetUserData(void *data) { m_userData = data; }

    void AppendBacktraceToElement(Logging::Element *el);
    void AppendCpuContextToElement(Logging::Element *el);
    void AppendArgumentsToElement(Logging::Element *el);
	OString ToString();

	virtual bool QueryForProperty(const OString &query, int &result);

protected:
	Function *m_function;
	void *m_backtraceAddress;
	void *m_returnAddress;
	CpuContext *m_cpuCtxLive;
	CpuContext m_cpuCtxEnter;
	CpuContext m_cpuCtxLeave;
	DWORD *m_lastErrorLive;

	OString m_argumentsData;
    ArgumentList *m_arguments;

    FunctionCallState m_state;

	bool m_shouldCarryOn;

    void *m_userData;

private:
    bool ShouldLogArgumentDeep(const Argument *arg) const;
    void AppendCpuRegisterToElement(Logging::Element *el, const char *name, DWORD value);
};

} // namespace InterceptPP
////////////////////////////////////////////////////////////////////////////////////////////////////
// File: JavascriptContext.cpp
// 
// Copyright 2010 Noesis Innovation Inc. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <msclr\lock.h>
#include <vcclr.h>
#include <msclr\marshal.h>

#include "JavascriptContext.h"

#include "SystemInterop.h"
#include "JavascriptException.h"
#include "JavascriptExternal.h"
#include "JavascriptInterop.h"

using namespace msclr;

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Noesis { namespace Javascript {

////////////////////////////////////////////////////////////////////////////////////////////////////

static DWORD curThreadId;

JavascriptContext::JavascriptContext()
{
	v8::Locker v8ThreadLock;
	mExternals = new vector<JavascriptExternal*>();
	mContext = new Persistent<Context>(Context::New());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

JavascriptContext::~JavascriptContext()
{
	v8::Locker v8ThreadLock;
	mContext->Dispose();
	Clear();
	delete mContext;
	delete mExternals;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void JavascriptContext::TerminateExecution()
{
	v8::V8::TerminateExecution();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void
JavascriptContext::SetParameter(System::String^ iName, System::Object^ iObject)
{
	pin_ptr<const wchar_t> namePtr = PtrToStringChars(iName);
	wchar_t* name = (wchar_t*) namePtr;
	JavascriptScope scope(this);
	HandleScope handleScope;
	
	Handle<Value> value = JavascriptInterop::ConvertToV8(iObject);
	(*mContext)->Global()->Set(String::New((uint16_t*)name), value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

System::Object^
JavascriptContext::GetParameter(System::String^ iName)
{
	pin_ptr<const wchar_t> namePtr = PtrToStringChars(iName);
	wchar_t* name = (wchar_t*) namePtr;
	JavascriptScope scope(this);
	HandleScope handleScope;
	
	Local<Value> value = (*mContext)->Global()->Get(String::New((uint16_t*)name));
	return JavascriptInterop::ConvertFromV8(value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

System::Object^
JavascriptContext::Run(System::String^ iScript)
{
	pin_ptr<const wchar_t> scriptPtr = PtrToStringChars(iScript);
	wchar_t* script = (wchar_t*)scriptPtr;
	JavascriptScope scope(this);
	SetStackLimit();
	HandleScope handleScope;
	Local<Value> ret;
	
	Local<Script> compiledScript = CompileScript(script);

	{
		TryCatch tryCatch;
		ret = (*compiledScript)->Run();

		if (ret.IsEmpty())
			throw gcnew JavascriptException(tryCatch);
	}
	
	return JavascriptInterop::ConvertFromV8(ret);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

System::Object^
JavascriptContext::Run(System::String^ iScript, System::String^ iScriptResourceName)
{
	pin_ptr<const wchar_t> scriptPtr = PtrToStringChars(iScript);
	wchar_t* script = (wchar_t*)scriptPtr;
	pin_ptr<const wchar_t> scriptResourceNamePtr = PtrToStringChars(iScriptResourceName);
	wchar_t* scriptResourceName = (wchar_t*)scriptResourceNamePtr;
	JavascriptScope scope(this);
	SetStackLimit();
	HandleScope handleScope;
	Local<Value> ret;	

	Local<Script> compiledScript = CompileScript(script);
	
	{
		TryCatch tryCatch;
		ret = (*compiledScript)->Run();

		if (ret.IsEmpty())
			throw gcnew JavascriptException(tryCatch);
	}
	
	return JavascriptInterop::ConvertFromV8(ret);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void
JavascriptContext::SetStackLimit()
{
    // v8 Needs to have its stack limit set separately in each thread because
	// it detects stack overflows by reference to a stack pointer that it
	// calculates when it is first invoked.  We recalculate the stack pointer
	// for each thread.
	DWORD dw = GetCurrentThreadId();
	if (dw != curThreadId) {
		v8::ResourceConstraints rc;
		int limit = (int)&rc - 500000;
		rc.set_stack_limit((uint32_t *)(limit));
		v8::SetResourceConstraints(&rc);
		curThreadId = dw;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

JavascriptContext^
JavascriptContext::GetCurrent()
{
	return sCurrentContext;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void
JavascriptContext::Enter()
{
	// We store the old context so that JavascriptContexts can be created and run
	// recursively.
	oldContext = sCurrentContext;
	sCurrentContext = this;
	(*mContext)->Enter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void
JavascriptContext::Exit()
{
	(*mContext)->Exit();
	sCurrentContext = oldContext;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void
JavascriptContext::Clear()
{
	while (mExternals->size())
	{
		delete mExternals->back();
		mExternals->pop_back();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Exposed for the benefit of a regression test.
void
JavascriptContext::Collect()
{
    while(!v8::V8::IdleNotification()) {}; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////

JavascriptExternal*
JavascriptContext::WrapObject(System::Object^ iObject)
{
	JavascriptExternal* external = new JavascriptExternal(iObject);
	mExternals->push_back(external);
	return external;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Local<Script>
CompileScript(wchar_t const *source_code)
{
	// convert source
	Local<String> source = String::New((uint16_t const *)source_code);

	// compile
	{
		TryCatch tryCatch;

		Local<Script> script = Script::Compile(source);

		if (script.IsEmpty())
			throw gcnew JavascriptException(tryCatch);

		return script;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} } // namespace Noesis::Javascript

////////////////////////////////////////////////////////////////////////////////////////////////////

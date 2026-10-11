// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// DEPRECATED:  This module declares the abstract interfaces underlying proto2
// RPC services.  These are intented to be independent of any particular RPC
// implementation, so that proto2 services can be used on top of a variety
// of implementations.  Starting with version 2.3.0, RPC implementations should
// not try to build on these, but should instead provide code generator plugins
// which generate code specific to the particular RPC implementation.  This way
// the generated code can be more appropriate for the implementation in use
// and can avoid unnecessary layers of indirection.
//
//
// When you use the protocol compiler to compile a service definition, it
// generates two classes:  An abstract interface for the service (with
// methods matching the service definition) and a "stub" implementation.
// A stub is just a type-safe wrapper around an RpcChannel which emulates a
// local implementation of the service.
//
// For example, the service definition:
//   service MyService {
//     rpc Foo(MyRequest) returns(MyResponse);
//   }
// will generate abstract interface "MyService" and class "MyService::Stub".
// You could implement a MyService as follows:
//   class MyServiceImpl : public MyService {
//    public:
//     MyServiceImpl() {}
//     ~MyServiceImpl() {}
//
//     // implements MyService ---------------------------------------
//
//     void Foo(google::protobuf::RpcController* controller,
//              const MyRequest* request,
//              MyResponse* response,
//              Closure* done) {
//       // ... read request and fill in response ...
//       done->Run();
//     }
//   };
// You would then register an instance of MyServiceImpl with your RPC server
// implementation.  (How to do that depends on the implementation.)
//
// To call a remote MyServiceImpl, first you need an RpcChannel connected to it.
// How to construct a channel depends, again, on your RPC implementation.
// Here we use a hypothentical "MyRpcChannel" as an example:
//   MyRpcChannel channel("rpc:hostname:1234/myservice");
//   MyRpcController controller;
//   MyServiceImpl::Stub stub(&channel);
//   FooRequest request;
//   FooRespnose response;
//
//   // ... fill in request ...
//
//   stub.Foo(&controller, request, &response, NewCallback(HandleResponse));
//
// On Thread-Safety:
//
// Different RPC implementations may make different guarantees about what
// threads they may run callbacks on, and what threads the application is
// allowed to use to call the RPC system.  Portable software should be ready
// for callbacks to be called on any thread, but should not try to call the
// RPC system from any thread except for the ones on which it received the
// callbacks.  Realistically, though, simple software will probably want to
// use a single-threaded RPC system while high-end software will want to
// use multiple threads.  RPC implementations should provide multiple
// choices.

#ifndef GOOGLE_PROTOBUF_SERVICE_H__
#define GOOGLE_PROTOBUF_SERVICE_H__

#include <string>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

class Service;
class RpcController;
class RpcChannel;

class Descriptor;
class ServiceDescriptor;
class MethodDescriptor;
class Message;

class LIBPROTOBUF_EXPORT Service {
 public:
  inline Service() {}
  virtual ~Service();

  enum ChannelOwnership {
    STUB_OWNS_CHANNEL,
    STUB_DOESNT_OWN_CHANNEL
  };

  virtual const ServiceDescriptor* GetDescriptor() = 0;

  virtual void CallMethod(const MethodDescriptor* method,
                          RpcController* controller,
                          const Message* request,
                          Message* response,
                          Closure* done) = 0;

  virtual const Message& GetRequestPrototype(
    const MethodDescriptor* method) const = 0;
  virtual const Message& GetResponsePrototype(
    const MethodDescriptor* method) const = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Service);
};

class LIBPROTOBUF_EXPORT RpcController {
 public:
  inline RpcController() {}
  virtual ~RpcController();

  virtual void Reset() = 0;

  virtual bool Failed() const = 0;

  virtual string ErrorText() const = 0;

  virtual void StartCancel() = 0;

  virtual void SetFailed(const string& reason) = 0;

  virtual bool IsCanceled() const = 0;

  virtual void NotifyOnCancel(Closure* callback) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RpcController);
};

class LIBPROTOBUF_EXPORT RpcChannel {
 public:
  inline RpcChannel() {}
  virtual ~RpcChannel();

  virtual void CallMethod(const MethodDescriptor* method,
                          RpcController* controller,
                          const Message* request,
                          Message* response,
                          Closure* done) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RpcChannel);
};

}

}
#endif

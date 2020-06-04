// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#include "NativeOutputRecord.hpp"  // my header
#include "Marshal.hpp"

namespace djinni_generated {

NativeOutputRecord::NativeOutputRecord() = default;

NativeOutputRecord::~NativeOutputRecord() = default;

auto NativeOutputRecord::fromCpp(JNIEnv* jniEnv, const CppType& c) -> ::djinni::LocalRef<JniType> {
    const auto& data = ::djinni::JniClass<NativeOutputRecord>::get();
    auto r = ::djinni::LocalRef<JniType>{jniEnv->NewObject(data.clazz.get(), data.jconstructor,
                                                           ::djinni::get(::djinni::I64::fromCpp(jniEnv, c.amount)),
                                                           ::djinni::get(::djinni::String::fromCpp(jniEnv, c.address)),
                                                           ::djinni::get(::djinni::String::fromCpp(jniEnv, c.label)),
                                                           ::djinni::get(::djinni::String::fromCpp(jniEnv, c.desc)),
                                                           ::djinni::get(::djinni::Bool::fromCpp(jniEnv, c.isMine)))};
    ::djinni::jniExceptionCheck(jniEnv);
    return r;
}

auto NativeOutputRecord::toCpp(JNIEnv* jniEnv, JniType j) -> CppType {
    ::djinni::JniLocalScope jscope(jniEnv, 6);
    assert(j != nullptr);
    const auto& data = ::djinni::JniClass<NativeOutputRecord>::get();
    return {::djinni::I64::toCpp(jniEnv, jniEnv->GetLongField(j, data.field_mAmount)),
            ::djinni::String::toCpp(jniEnv, (jstring)jniEnv->GetObjectField(j, data.field_mAddress)),
            ::djinni::String::toCpp(jniEnv, (jstring)jniEnv->GetObjectField(j, data.field_mLabel)),
            ::djinni::String::toCpp(jniEnv, (jstring)jniEnv->GetObjectField(j, data.field_mDesc)),
            ::djinni::Bool::toCpp(jniEnv, jniEnv->GetBooleanField(j, data.field_mIsMine))};
}

}  // namespace djinni_generated

#pragma once
// GTAVXR-owned C++11/v110 helper. The supplied device must use the SAME context
// mutex as native render/Present, not a bridge-private mutex or per-call D3D lock.
// build146 P: optional measurement hook around the lock acquisition only (NULL = none; set by the native adapter).
struct GTAVXRContextWaitHook
{
    static void (*&fn())(bool)
    {
        static void (*hook)(bool) = 0;
        return hook;
    }
};
template <class Device> class GTAVXRContextScope
{
    Device& device;
    GTAVXRContextScope(const GTAVXRContextScope&);
    GTAVXRContextScope& operator=(const GTAVXRContextScope&);

  public:
    explicit GTAVXRContextScope(Device& d) : device(d)
    {
        void (*hook)(bool) = GTAVXRContextWaitHook::fn();
        if (hook)
            hook(true);
        device.LockContext();
        if (hook)
            hook(false);
    }
    ~GTAVXRContextScope()
    {
        device.UnlockContext();
    }
};
template <class Device, class Call> int GTAVXRWithContext(Device& device, Call call)
{
    GTAVXRContextScope<Device> scope(device);
    return call();
}

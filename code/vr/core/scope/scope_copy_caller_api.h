#pragma once
#include "scope_copy_transport.h"
 
namespace GTAVXRNativePair
{
void RecordScopeCopyMode(const gx_scope_copy::Key&, gx_scope_copy::Mode requested, bool firstCopy,
                         uintptr_t sourceSkeleton);
gx_scope_transport::Capture CaptureScopeCopyMode(const gx_scope_copy::Key&);
}

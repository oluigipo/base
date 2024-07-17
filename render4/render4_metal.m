#include "common.h"
#include "api_os.h"

#include <TargetConditionals.h>
#include <AvailabilityMacros.h>
#import <Metal/Metal.h>
// #import <QuartzCore/CoreAnimation.h>
#include "api_render4.h"

struct R4_Context
{
	id<MTLDevice> device;
	// id<CAMetalDrawable> drawable;
}
typedef R4_Context;

API R4_Context*
R4_MTL_MakeContext(Arena* arena, R4_ContextDesc const* desc)
{
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();

	return ArenaPushStructInit(arena, R4_Context, {
		.device = device,
	});

lbl_error:
	[device release];
	return NULL;
}

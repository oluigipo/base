//+private
package testodin

import "base:runtime"
import C "src:common_odin"
import R3 "src:render3"

@(private="file")
Main :: proc()
{
    using C
    arena := OS_VirtualAllocArena(256<<20)
    OS_Init(OS_Init_WindowAndGraphics)

    window := OS_CreateWindow({
        width = 1280,
        height = 720,
        title = "test_odin",
        resizeable = true,
    })
    r3 := R3.D3D11_MakeContext(&arena, {
        window = &window,
    })

    running := true
    for running {
        scratch := ArenaSave(OS_ScratchArena(nil, 0))
        defer ArenaRestore(scratch)

        // Event polling
        event_count: int
        events := cast([^]OS_Event) OS_PollEvents(false, scratch.arena, &event_count)
        for i in 0..<event_count {
            event := &events[i]
            if event.kind == .WindowClose {
                running = false
            }
        }

        // Rendering
        R3.SetViewports(r3, 1, &R3.Viewport{
            width = 1280,
            height = 720,
            max_depth = 1.0,
        })
        R3.SetRenderTarget(r3, nil)
        R3.Clear(r3, {
            flag_color = true,
            color = { 0.2, 0.0, 0.3, 1.0 },
        })
        R3.Present(r3)
    }
}

@(link_name="EntryPoint", require, linkage="strong")
EntryPoint :: proc "c"(argc: i32, argv: [^]cstring) -> i32
{
    runtime.args__ = argv[:argc]
    context = runtime.default_context()
    #force_no_inline runtime._startup_runtime()

    Main()

    #force_no_inline runtime._cleanup_runtime()
    return 0
}

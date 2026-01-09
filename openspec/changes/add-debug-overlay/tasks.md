# Tasks

- [x] Add frame time ring buffer to ImGuiLayer (render and source)
- [x] Add frame time tracking in begin_frame()
- [x] Add notify_source_frame() method for source FPS tracking
- [x] Add draw_debug_overlay() method with FPS/frame time/graph
- [x] Wire notify_source_frame through UiController
- [x] Call notify_source_frame from Application on new frame (using frame_number)
- [x] Fix vk_layer to use CaptureFrameMetadata with frame_number
- [x] Add separate plots for render and source frame times
- [x] Remove verbose DMA-BUF log
- [x] Test overlay display
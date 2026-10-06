import sys

from Swabian import TimeTagger as tt

sys.path.insert(0, 'build_debug/python')


import grater as gr

tagger = tt.createTimeTaggerNetwork('78.104.11.122')


# def make_pattern_counter(
#     tagger, channels, coincidence_window, buffer_size, max_length=2
# ):
# 
#     return gr.PatternCounter(
#         int(tagger.this),
#         tagger,
#         channels,
#         coincidence_window,
#         buffer_size,
#         max_length,
#     )


try:
    pc = gr.PatternCounter(
        tagger,
        list(range(1, 18)),
        1000,
        gr.BufferSize.Medium,
        2,
    )
    print('constructed ok:', pc)
except Exception:
    import traceback

    traceback.print_exc()

for i in range(100):
    pc.startFor(int(2e12))
    pc.waitUntilFinished()
    for pattern, count in pc.get_data():
        if len(pattern) > 1 and count > 0:
            print(pattern, count)

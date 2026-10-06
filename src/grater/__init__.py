def make_pattern_counter(
    tagger, channels, coincidence_window, buffer_size, max_length=2
):
    from grater import PatternCounter

    return PatternCounter(
        int(tagger.this),
        tagger,
        channels,
        coincidence_window,
        buffer_size,
        max_length,
    )

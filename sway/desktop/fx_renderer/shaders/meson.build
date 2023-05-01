embed = find_program('./embed.sh', native: true)

shaders = [
    'blur1.frag',
    'blur2.frag',
    'box_shadow.frag',
    'common.vert',
    'corner.frag',
    'quad.frag',
    'quad_round.frag',
    'stencil_mask.frag',
    'tex.frag',
]

foreach name : shaders
    output = name.underscorify() + '_src.h'
    var = name.underscorify() + '_src'
    sway_sources += custom_target(
        output,
        command: [embed, var],
        input: name,
        output: output,
        feed: true,
        capture: true,
    )
endforeach

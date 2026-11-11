KERNEL: DENOISE2D
ITERATE: 1
COUNT: 1
REPEAT: 1
input source(1024, 1024)
output product(0, 0) = (source(0, 0) - source(0, -1)) * (source(0, 0) - source(0, -1)) + (source(0, 0) - source(0, 1)) * (source(0, 0) - source(0, 1)) + (source(0, 0) - source(-1, 0)) * (source(0, 0) - source(-1, 0)) + (source(0, 0) - source(1, 0)) * (source(0, 0) - source(1, 0))
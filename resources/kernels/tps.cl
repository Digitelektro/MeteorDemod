// featuring floating point bit level hacking, Remez algorithm
//  x=m*2^p => ln(x)=ln(m)+ln(2)p
inline float ln(float x) {
    uint bx = *(uint*)(&x);
    const uint ex = bx >> 23;
    int t = (int)ex - (int)127;
    uint s = (t < 0) ? (-t) : t;
    bx = 1065353216 | (bx & 8388607);
    x = *(float*)(&bx);
    return -1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * x) * x) * x) * x + 0.6931471806f * t;
}

inline float pow2(const float x) {
    return x * x;
}

inline float dist(const float x1, const float y1, const float x2, const float y2) {
    float norm = pow2(x1 - x2) + pow2(y1 - y2);
    if(norm < 0) {
        norm = 0;
    }
    // return sqrt(norm);
    return norm * log(norm + FLT_EPSILON);
    // return norm * ln(norm + FLT_EPSILON);
}

void kernel apply(global const float* shapeRef, global const float* tpsParameters, global const float* point, const uint shapeRefRows, const uint tpsParamRows, const uint cols, global float* out) {
    int id = get_global_id(0);
    int size = get_global_size(0);

    int itemPerTask = (cols + size - 1) / size;
    int start = id * itemPerTask;
    int end = (id + 1) * itemPerTask;
    if(end > cols) {
        end = cols;
    };

    for(int x = start; x < end; x++) {
        for(int i = 0; i < 2; i++) {
            const float a1 = tpsParameters[(tpsParamRows - 3) * 2 + i];
            const float ax = tpsParameters[(tpsParamRows - 2) * 2 + i];
            const float ay = tpsParameters[(tpsParamRows - 1) * 2 + i];

            const float affine = a1 + ax * point[x * 2] + ay * point[x * 2 + 1];
            float nonrigid = 0;

            for(int j = 0; j < shapeRefRows; j++) {
                nonrigid += tpsParameters[j * 2 + i] * dist(shapeRef[j * 2], shapeRef[j * 2 + 1], point[x * 2], point[x * 2 + 1]);
            }
            if(i == 0) {
                out[x * 2] = affine + nonrigid;
            }
            if(i == 1) {
                out[x * 2 + 1] = affine + nonrigid;
            }
        }
    }
}
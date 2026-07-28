/*
 * eigen_native.c - Template linear algebra (Eigen subset)
 * Tier 4: scientific computing
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xeigen_matrix_type(long long id) {
    switch (id) {
        case 0: return "MatrixXd";
        case 1: return "MatrixXf";
        case 2: return "MatrixXi";
        case 3: return "VectorXd";
        case 4: return "VectorXf";
        case 5: return "RowVectorXd";
        case 6: return "Matrix2d";
        case 7: return "Matrix3d";
        case 8: return "Matrix4d";
        case 9: return "Quaterniond";
        default: return "Unknown";
    }
}

const char* xeigen_decomposition(long long id) {
    switch (id) {
        case 0: return "LU";
        case 1: return "QR";
        case 2: return "SVD";
        case 3: return "Cholesky";
        case 4: return "EigenSolver";
        case 5: return "SelfAdjointEigenSolver";
        case 6: return "JacobiSVD";
        case 7: return "HouseholderQR";
        case 8: return "ColPivHouseholderQR";
        case 9: return "FullPivLU";
        default: return "Unknown";
    }
}

const char* xeigen_storage_order(long long id) {
    switch (id) {
        case 0: return "col-major";
        case 1: return "row-major";
        default: return "Unknown";
    }
}

/* 2x2 matrix inverse: input "a,b,c,d" => returns "d/det,-b/det,-c/det,a/det" */
const char* xeigen_inverse2x2(const char *mat) {
    double m[4];
    const char *p = mat;
    int i;
    for (i = 0; i < 4; i++) {
        m[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    double det = m[0]*m[3] - m[1]*m[2];
    if (fabs(det) < 1e-15) return "singular";
    double inv = 1.0 / det;
    sprintf(buf, "%g,%g,%g,%g", m[3]*inv, -m[1]*inv, -m[2]*inv, m[0]*inv);
    return buf;
}

/* 2x2 matrix multiply: A*B where A="a,b,c,d" B="e,f,g,h" */
const char* xeigen_multiply2x2(const char *a, const char *b) {
    double ma[4], mb[4];
    const char *p;
    int i;
    p = a;
    for (i = 0; i < 4; i++) {
        ma[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    p = b;
    for (i = 0; i < 4; i++) {
        mb[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    /* [a b] [e f]   [ae+bg  af+bh]
       [c d] [g h] = [ce+dg  cf+dh] */
    sprintf(buf, "%g,%g,%g,%g",
        ma[0]*mb[0]+ma[1]*mb[2], ma[0]*mb[1]+ma[1]*mb[3],
        ma[2]*mb[0]+ma[3]*mb[2], ma[2]*mb[1]+ma[3]*mb[3]);
    return buf;
}

/* Transpose 2x2: "a,b,c,d" => "a,c,b,d" */
const char* xeigen_transpose2x2(const char *mat) {
    double m[4];
    const char *p = mat;
    int i;
    for (i = 0; i < 4; i++) {
        m[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    sprintf(buf, "%g,%g,%g,%g", m[0], m[2], m[1], m[3]);
    return buf;
}

const char* xeigen_solver_type(long long id) {
    switch (id) {
        case 0: return "direct";
        case 1: return "iterative";
        case 2: return "sparse";
        default: return "Unknown";
    }
}

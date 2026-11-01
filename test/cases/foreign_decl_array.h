#ifndef FOREIGN_DECL_ARRAY_H
#define FOREIGN_DECL_ARRAY_H
typedef struct foreign_decl_matrix { float m[16]; } foreign_decl_matrix;
typedef struct foreign_decl_vertices { foreign_decl_matrix transforms[3]; } foreign_decl_vertices;
foreign_decl_matrix foreign_decl_matrix_data(void);
int foreign_decl_matrix_sum(foreign_decl_matrix value);
void foreign_decl_array_expect(int actual, int expected);
#endif

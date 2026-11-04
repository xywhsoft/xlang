#ifndef XL6_TEST_FOREIGN_AGGREGATE_PLACE_H
#define XL6_TEST_FOREIGN_AGGREGATE_PLACE_H
typedef struct PlaceVertex { float x,y; } PlaceVertex;
typedef struct PlaceQuad { PlaceVertex vertices[4]; } PlaceQuad;
typedef struct PlaceModel { PlaceQuad items[2]; int tag; } PlaceModel;
PlaceModel aggregate_place_model(void);
#endif

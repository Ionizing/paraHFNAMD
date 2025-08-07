#ifndef EFIELD_H
#define EFIELD_H

#include <string>
#include <vector>
#include "const.h"

typedef struct {
    double x;
    double y;
    double z;
} EField;

extern std::vector<EField> efields;

void init_engine(const std::string& fname);
EField get_efield(const double t);
std::vector<EField> get_efield_array(const std::vector<double>& ts);
void set_efield_array(const std::vector<double>& ts);
void destroy_engine();

#endif // EFIELD_H

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

// No need to export these symbols
//void init_engine(const std::string& fname);
//EField get_efield(const double t);
//std::vector<EField> get_efield_array(const std::vector<double>& ts);
//void set_efield_array(const std::vector<double>& ts);
//void destroy_engine();

void init_efield(const std::string& jsfname, int namdtim, int neleint);
void write_efield(const std::string& fname);

#endif // EFIELD_H

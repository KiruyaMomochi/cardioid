#ifndef TT06FUNC_HH
#define TT06FUNC_HH
#include <map>
#include <string>
#include <vector>
#include "pade.hh" 
#include "TT06Func.hh" 
#define SQ(x) ((x)*(x))
#define CUBE(x) ((x)*(x)*(x))

using namespace std; 
/*
 * RATES[0] is d/dt V in component membrane (millivolt).
 * RATES[4] is d/dt Xr1 in component rapid_time_dependent_potassium_current_Xr1_gate (dimensionless).
 * RATES[5] is d/dt Xr2 in component rapid_time_dependent_potassium_current_Xr2_gate (dimensionless).
 * RATES[6] is d/dt Xs in component slow_time_dependent_potassium_current_Xs_gate (dimensionless).
 * RATES[7] is d/dt m in component fast_sodium_current_m_gate (dimensionless).
 * RATES[8] is d/dt h in component fast_sodium_current_h_gate (dimensionless).
 * RATES[9] is d/dt j in component fast_sodium_current_j_gate (dimensionless).
 * RATES[11] is d/dt d in component L_type_Ca_current_d_gate (dimensionless).
 * RATES[12] is d/dt f in component L_type_Ca_current_f_gate (dimensionless).
 * RATES[13] is d/dt f2 in component L_type_Ca_current_f2_gate (dimensionless).
 * RATES[14] is d/dt fCass in component L_type_Ca_current_fCass_gate (dimensionless).
 * RATES[15] is d/dt s in component transient_outward_current_s_gate (dimensionless).
 * RATES[16] is d/dt r in component transient_outward_current_r_gate (dimensionless).
 * RATES[18] is d/dt R_prime in component calcium_dynamics (dimensionless).
 * RATES[3] is d/dt Ca_i in component calcium_dynamics (millimolar).
 * RATES[17] is d/dt Ca_SR in component calcium_dynamics (millimolar).
 * RATES[10] is d/dt Ca_ss in component calcium_dynamics (millimolar).
 * RATES[2] is d/dt Na_i in component sodium_dynamics (millimolar).
 * RATES[1] is d/dt K_i in component potassium_dynamics (millimolar).
*/
// int mapCell2Dev[]                  {1,2,3,10,17,18,14,7,8,9,4,5,6,16,11,12,13,15};
namespace TT06Func
{

enum TT06STATE { Ca_i, K_i, Na_i, Ca_ss, Ca_SR, R_prime, fCass, m_gate, h_gate, j_gate, Xr1_gate, Xr2_gate, Xs_gate, r_gate, d_gate, f_gate, f2_gate,  jL_gate, s_gate, nStateVar} ; 
enum STATETYPE { nonGateVar, GateVar }; 

#define gateFitOffset 7
#define nGateVar 12 
#define gateOffset 7
#define dVK_i   K_i 

struct STATE { int index, type; double value;}; 

struct CellTypeParmsFull { string name; vector<int> anatomyIndices; int s_switch; double P_NaK, g_Ks, g_to, g_NaL, Vm; map<string,STATE> state;}  ;
struct CellTypeParms     { int cellType, s_switch; double P_NaK, g_Ks, g_to, g_NaL, minK_i,maxK_i,midK_i,minNa_i,maxNa_i,midNa_i;}  ;
struct TT06DevState { double state[nStateVar]; };

void initCnst();
void updateNonGate(double dt, CellTypeParms *cellTypeParms, int n, int *cellType, const double *Vm, int offset, double **state, double *dVdt);
void updateGate(double dt, int n, int *cellType, const double *Vm, int offset, double **state);
double get_c9();
PADE **makeFit(double tol, double V0, double V1, double deltaV, int mod);
void writeFit(PADE **fit); 
map<string,CellTypeParmsFull> getStandardCellTypes();
};

#endif

// IpCa = Cai::GpCa:
#include <math.h>
#include "OHaraRudy.h"
#include "OHaraRudy_IpCa.h"

void OHaraRudy_IpCaFunc(CELLPARMS *parmsPtr, double *cell, int pOffset, DERIVED *derived, double dt )
{
   CONCENTRATIONS   *concentrations = (CONCENTRATIONS*) (cell + CONCENTRATIONS_OFFSET); 
   PARAMETERS *cP  = (PARAMETERS *)parmsPtr; 
   double Cai = concentrations->Cai; 
   derived->I.pCa = cP->GpCa * Cai/(0.0005 + Cai); 
}

//LatLong- UTM conversion..h
//definitions for lat/long to UTM and UTM to lat/lng conversions
#include <ns3/object.h>
#include <string.h>

#ifndef LATLONGCONV
#define LATLONGCONV
//namespace ns3 {

//class LatLongUTMconversion{
//  public:
    //static TypeId GetTypeId (void); //recommended by ns-3 tutorial

    void LLtoUTM(int ReferenceEllipsoid, const double Lat, const double Long, 
			 double &UTMNorthing, double &UTMEasting, char &UTMZone, int &ZoneNumber);
    void UTMtoLL(int ReferenceEllipsoid, const double UTMNorthing, const double UTMEasting, const char* UTMZone,
			  double& Lat,  double& Long );
    char UTMLetterDesignator(double Lat);
    double haversine(const double lon1, const double lat1, const double lon2, const double lat2);
    //void LLtoSwissGrid(const double Lat, const double Long, 
//			 double &SwissNorthing, double &SwissEasting);
    //void SwissGridtoLL(const double SwissNorthing, const double SwissEasting, 
//					double& Lat, double& Long);
//};
//}
class Ellipsoid
{
public:
	Ellipsoid(){};
	Ellipsoid(int Id, std::string name, double radius, double ecc)
	{
		id = Id; ellipsoidName = name; 
		EquatorialRadius = radius; eccentricitySquared = ecc;
	}

	int id;
	std::string ellipsoidName;
	double EquatorialRadius; 
	double eccentricitySquared;  

};



#endif

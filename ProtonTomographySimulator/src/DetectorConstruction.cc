#include <algorithm>
#include "DetectorConstruction.hh"

#include "G4FieldManager.hh"
#include "G4TransportationManager.hh"
#include "G4Mag_UsualEqRhs.hh"

#include "G4Material.hh"
#include "G4Element.hh"
#include "G4MaterialTable.hh"
#include "G4NistManager.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4SystemOfUnits.hh"

#include "G4VSolid.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4Para.hh"
#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4PVParameterised.hh"
#include "G4UserLimits.hh"

#include "G4SDManager.hh"
#include "G4VSensitiveDetector.hh"
#include "G4RunManager.hh"

#include "G4ios.hh"
#include "G4VisAttributes.hh"

#include "G4PVReplica.hh"

#include "G4SubtractionSolid.hh"



//----------------------------------------------------------------------//
// Constructor                                                          //
//----------------------------------------------------------------------//
DetectorConstruction::DetectorConstruction(ConfigurationGeometry *w) {
    myConf = w;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Destructor                                                           //
//----------------------------------------------------------------------//
DetectorConstruction::~DetectorConstruction() {

    DestroyMaterials();

}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Creates all the geometrical structures                               //
//----------------------------------------------------------------------//
G4VPhysicalVolume* DetectorConstruction::Construct() {

    //Building the materials
    ConstructMaterials();

    //Printing the geometry
    myConf->Print();

    //Manager of objects in memory
    G4SDManager* SDman = G4SDManager::GetSDMpointer();
    G4String SDname;

    //Creating the world
    G4VSolid* worldSolidPrim = new G4Box("worldBoxPrim", 1.1 * myConf->getSizeX()/2.0 , 1.1 * myConf->getSizeY() / 2.0 , 1.1 * myConf->getSizeZ()/2.0 );
    G4LogicalVolume* worldLogicalPrim = new G4LogicalVolume(worldSolidPrim, materials["air"], "worldLogicalPrim",0,0,0);
    G4VPhysicalVolume* worldPhysicalPrim = new G4PVPlacement(0, G4ThreeVector(), worldLogicalPrim, "worldPhysicalPrim", 0, 0, 0);

    G4VSolid* worldSolid = new G4Box("worldBox", myConf->getSizeX()/2.0 , myConf->getSizeY()/2.0 , myConf->getSizeZ()/2.0 );
    G4LogicalVolume* worldLogical = new G4LogicalVolume(worldSolid, materials["air"], "worldLogical",0,0,0);
    G4VPhysicalVolume* worldPhysical = new G4PVPlacement(0, G4ThreeVector(0, 0, 0), worldLogical, "worldPhysical", worldLogicalPrim, false, 0);

    //Both worlds are drawn as wireframe so the inner volumes are visible
    G4VisAttributes *worldVisAtt = new G4VisAttributes();
    worldVisAtt->SetForceWireframe(true);
    worldLogicalPrim->SetVisAttributes(worldVisAtt);
    worldLogical->SetVisAttributes(worldVisAtt);

    myConf->createG4objects(worldLogical, materials, SDman);
 
    DumpGeometricalTree(worldPhysicalPrim, 3);
    
    return worldPhysicalPrim;

}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Construct all the materials                                          //
//----------------------------------------------------------------------//
void DetectorConstruction::ConstructMaterials() {

    G4NistManager* man = G4NistManager::Instance();

    materials.insert(std::pair<G4String, G4Material *>("air", man->FindOrBuildMaterial("G4_AIR")));
    materials.insert(std::pair<G4String, G4Material *>("iron", man->FindOrBuildMaterial("G4_Fe")));
    materials.insert(std::pair<G4String, G4Material *>("uranium", man->FindOrBuildMaterial("G4_U")));
    materials.insert(std::pair<G4String, G4Material *>("aluminium", man->FindOrBuildMaterial("G4_Al")));
    materials.insert(std::pair<G4String, G4Material *>("carbon", man->FindOrBuildMaterial("G4_C")));
    materials.insert(std::pair<G4String, G4Material *>("argon", man->FindOrBuildMaterial("G4_Ar")));
    materials.insert(std::pair<G4String, G4Material *>("lead", man->FindOrBuildMaterial("G4_Pb")));
    materials.insert(std::pair<G4String, G4Material *>("silicon", man->FindOrBuildMaterial("G4_Si")));
    materials.insert(std::pair<G4String, G4Material *>("steel", man->FindOrBuildMaterial("G4_STAINLESS-STEEL")));
    materials.insert(std::pair<G4String, G4Material *>("lung", man->FindOrBuildMaterial("G4_LUNG_ICRP")));
    materials.insert(std::pair<G4String, G4Material *>("bone", man->FindOrBuildMaterial("G4_BONE_COMPACT_ICRU")));
    materials.insert(std::pair<G4String, G4Material *>("fat", man->FindOrBuildMaterial("G4_ADIPOSE_TISSUE_ICRP")));
    materials.insert(std::pair<G4String, G4Material *>("brain", man->FindOrBuildMaterial("G4_BRAIN_ICRP")));

    // Add the optical properties to the materials already defined in G4
    // Define a vector of energies in eV corresponding to the wavelengths of interest for optical properties: 500, 460, 435, 400, 380 nm (E = 1239.84/λ)
    std::vector<G4double> E = {2.48*eV, 2.70*eV, 2.85*eV, 3.10*eV, 3.26*eV};

    // OPTICAL PROPERTIES MUST BE CHANGED DEPENDING ON THE FIBER MATERIALS! (Sería buena idea ponerlo en el archivo de configuración?)

    // Core: Scintillating polystyrene
    G4Material* core = man->FindOrBuildMaterial("G4_POLYSTYRENE");
    auto* coreMPT = new G4MaterialPropertiesTable();
    coreMPT->AddProperty("RINDEX",                  E, {1.59, 1.59, 1.59, 1.59, 1.59}); // Refractive index of the core material is constant for all energies
    coreMPT->AddProperty("ABSLENGTH",               E, {3.5*m, 3.5*m, 3.0*m, 1.0*m, 0.5*m});
    coreMPT->AddProperty("SCINTILLATIONCOMPONENT1", E, {0.10, 0.60, 1.00, 0.40, 0.05}); // Emission spectrum of the scintillator (normalized to 1)
    coreMPT->AddConstProperty("SCINTILLATIONYIELD",         1000./MeV);  // Number of photons per MeV deposited; The real one is 8000; low for testing
    coreMPT->AddConstProperty("RESOLUTIONSCALE",            1.0); // Controls the intrinsic dispersion/variance in photon production (var = R * mean); 1.0 = Poisson statistics; 0.0 = no variance; 
    coreMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 3.2*ns); // Scintillating time decay constant
    core->SetMaterialPropertiesTable(coreMPT); 
    core->GetIonisation()->SetBirksConstant(0.126*mm/MeV); // Light yield per path length as a function of the energy loss per path length for a particle traversing a scintillator
    materials.insert(std::pair<G4String, G4Material *>("scint_core", core));


    // Definition of the spectrum of the scintillaion light of the EJ-204 scintillator (from the datasheet of Eljen Technology)

    std::vector<G4double> E_ej204 = {
    3.2627*eV, 3.2384*eV, 3.2144*eV, 3.1908*eV, 3.1675*eV, 3.1445*eV, 3.1219*eV, 3.0996*eV, 3.0776*eV, 3.0559*eV,
    3.0346*eV, 3.0135*eV, 2.9927*eV, 2.9722*eV, 2.9520*eV, 2.9321*eV, 2.9124*eV, 2.8930*eV, 2.8738*eV, 2.8549*eV,
    2.8362*eV, 2.8178*eV, 2.7996*eV, 2.7817*eV, 2.7640*eV, 2.7465*eV, 2.7292*eV, 2.7122*eV, 2.6953*eV, 2.6787*eV,
    2.6622*eV, 2.6460*eV, 2.6300*eV, 2.6141*eV, 2.5985*eV, 2.5830*eV, 2.5677*eV, 2.5526*eV, 2.5377*eV, 2.5229*eV,
    2.5083*eV, 2.4939*eV, 2.4797*eV, 2.4656*eV, 2.4517*eV, 2.4379*eV, 2.4243*eV, 2.4108*eV, 2.3975*eV, 2.3843*eV
    };

    std::vector<G4double> amplitudes_ej204 = {
    0.0657, 0.1114, 0.1783, 0.2697, 0.3854, 0.5205, 0.6642, 0.8009, 0.9124, 0.9822,
    1.0000, 0.9944, 0.9810, 0.9602, 0.9323, 0.8980, 0.8581, 0.8134, 0.7650, 0.7138,
    0.6606, 0.6066, 0.5526, 0.4994, 0.4477, 0.3982, 0.3514, 0.3076, 0.2671, 0.2301,
    0.1967, 0.1667, 0.1403, 0.1170, 0.0969, 0.0796, 0.0648, 0.0524, 0.0420, 0.0334,
    0.0264, 0.0207, 0.0160, 0.0124, 0.0094, 0.0072, 0.0054, 0.0040, 0.0030, 0.0022
    };

    std::vector<G4double> rindex_ej204 = {
    1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59,
    1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59,
    1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59,
    1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59,
    1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59, 1.59,
    };

    std::vector<G4double> longitudes_absorcion_cm = {
    0.25*cm,   0.58*cm,   1.42*cm,   3.11*cm,   6.34*cm,  12.12*cm,  21.80*cm,  36.54*cm,  56.44*cm,  80.35*cm,
    106.11*cm, 131.02*cm, 152.41*cm, 168.41*cm, 178.50*cm, 183.33*cm, 184.34*cm, 183.15*cm, 180.95*cm, 178.61*cm,
    176.62*cm, 175.22*cm, 174.43*cm, 174.13*cm, 174.18*cm, 174.45*cm, 174.83*cm, 175.24*cm, 175.63*cm, 175.97*cm,
    176.24*cm, 176.45*cm, 176.61*cm, 176.71*cm, 176.78*cm, 176.81*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm,
    176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm, 176.83*cm
    };


    // Geant4 requires the energies of a property vector in increasing order (the data above is sorted by increasing wavelength)
    std::reverse(E_ej204.begin(), E_ej204.end());
    std::reverse(amplitudes_ej204.begin(), amplitudes_ej204.end());
    std::reverse(rindex_ej204.begin(), rindex_ej204.end());
    std::reverse(longitudes_absorcion_cm.begin(), longitudes_absorcion_cm.end());

    // Core material: EJ-204 (https://eljentechnology.com/images/products/data_sheets/EJ-200_EJ-204_EJ-208_EJ-212.pdf)
    // EJ-204 is polyvinyltoluene-based. It must be a different G4Material than "scint_core" (G4_POLYSTYRENE); otherwise both share the same pointer and the properties table is overwritten
    G4Material* EJ204 = man->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
    auto* EJ204MPT = new G4MaterialPropertiesTable();
    EJ204MPT->AddProperty("RINDEX", E_ej204, rindex_ej204);
    EJ204MPT->AddProperty("SCINTILLATIONCOMPONENT1", E_ej204, amplitudes_ej204);
    EJ204MPT->AddProperty("ABSLENGTH", E_ej204, longitudes_absorcion_cm);
    EJ204MPT->AddConstProperty("SCINTILLATIONYIELD", 10400./MeV);  // Number of photons per MeV deposited;
    EJ204MPT->AddConstProperty("RESOLUTIONSCALE", 1.0); // Controls the intrinsic dispersion/variance in photon production (var = R * mean); 1.0 = Poisson statistics; 0.0 = no variance;
    EJ204MPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 1.8*ns); // Scintillating time decay constant
    EJ204MPT->AddConstProperty("SCINTILLATIONRISETIME1",     0.7*ns);     // Opcional: tiempo de subida rápido
    EJ204->SetMaterialPropertiesTable(EJ204MPT); 
    EJ204->GetIonisation()->SetBirksConstant(0.126*mm/MeV); // Light yield per path length as a function of the energy loss per path length for a particle traversing a scintillator
    materials.insert(std::pair<G4String, G4Material *>("EJ204", EJ204));


    // Cladding: PMMA (plexiglass) (Not scintillating; refractive index lower than the core to ensure total internal reflection)
    G4Material* clad = man->FindOrBuildMaterial("G4_PLEXIGLASS");
    auto* cladMPT = new G4MaterialPropertiesTable();
    cladMPT->AddProperty("RINDEX", E, {1.49, 1.49, 1.49, 1.49, 1.49});
    clad->SetMaterialPropertiesTable(cladMPT);
    materials.insert(std::pair<G4String, G4Material *>("scint_clad", clad));

    // Air with RINDEX=1 so that light can exit the fiber without dying at the boundary. Add the optical properties of air
    auto* airMPT = new G4MaterialPropertiesTable();
    airMPT->AddProperty("RINDEX", E, {1.0, 1.0, 1.0, 1.0, 1.0});
    materials["air"]->SetMaterialPropertiesTable(airMPT);
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Destroy all the materials                                            //
//----------------------------------------------------------------------//
void DetectorConstruction::DestroyMaterials() {
    // Destroy all allocated elements and materials
    size_t i;
    G4MaterialTable* matTable = (G4MaterialTable*)G4Material::GetMaterialTable();
    for(i=0; i<matTable->size(); i++) delete (*(matTable))[i];
    matTable->clear();
    G4ElementTable* elemTable = (G4ElementTable*)G4Element::GetElementTable();
    for(i=0; i<elemTable->size(); i++) delete (*(elemTable))[i];
    elemTable->clear();

}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


void DetectorConstruction::DumpGeometricalTree(G4VPhysicalVolume* aVolume,G4int depth)
{

    for(int isp=0; isp<depth; isp++)
    {
        G4cout << "  ";
    }
    G4cout << aVolume->GetName() << "[" << aVolume->GetCopyNo() << "] "
           << aVolume->GetLogicalVolume()->GetName() << " "
           << aVolume->GetLogicalVolume()->GetNoDaughters() << " "
           << aVolume->GetLogicalVolume()->GetMaterial()->GetName();
    if(aVolume->GetLogicalVolume()->GetSensitiveDetector())
    {
        G4cout << " " << aVolume->GetLogicalVolume()->GetSensitiveDetector()->GetFullPathName();
    }
    G4cout << G4endl;
    for(int i=0; i<aVolume->GetLogicalVolume()->GetNoDaughters(); i++)
    {
        DumpGeometricalTree(aVolume->GetLogicalVolume()->GetDaughter(i),depth+1);
    }

}

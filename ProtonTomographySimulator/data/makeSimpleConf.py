import json, sys, optparse
import copy



if __name__=='__main__':

    parser = optparse.OptionParser(usage='usage: %prog [options] path', version='%prog 1.0')
    parser.add_option('-i', '--input', action='store', type='string', dest='inputFile', default='confExample.json', help='Configuration file template')
    parser.add_option('-o', '--output', action='store', type='string', dest='outputFile', default='conf.json', help='Final configuration file')

    (opts, args) = parser.parse_args()

    with open(opts.inputFile, 'r') as file:
        data = json.load(file)

    theWorld = copy.copy(data['theWorld'])
    theBeam = copy.copy(data['theBeam'])
    phantom = copy.copy(data['Phantoms'][0])
    detector = copy.copy(data['Detectors'][0])
    layer = copy.copy(data['Detectors'][0]['Layers'][0])
    fiber = copy.copy(data['Detectors'][0]['Layers'][0]['Fibers'][0])
    photons = copy.copy(data['Photons'])
    
    #Modify world directly in data
    theWorld['xSizeWorld'] = 300
    theWorld['ySizeWorld'] = 300
    theWorld['zSizeWorld'] = 300
    #Modify beam directly in data
    theBeam['xBeamPosition'] = 0
    theBeam['yBeamPosition'] = 0
    theBeam['zBeamPosition'] = 50
    theBeam['xBeamSigma'] = 0.0
    theBeam['yBeamSigma'] = 0.0
    theBeam['xDir'] = 0
    theBeam['yDir'] = 0
    theBeam['zDir'] = 0
    theBeam['maxOpenAngle'] = 0.0
    theBeam['nStep'] = 100
    theBeam['nParticles'] = 1
    theBeam['nParticlesDistribution'] = "Constant"
    theBeam['energy'] = 100.0
    theBeam['energyDistribution'] = "Constant"
    theBeam['energySigma'] = 1.0
    theBeam['tBeamSigma'] = 3.0
    #Modify phantoms directly in data
    thePhantoms = []
    phantom['name'] = "air"
    phantom['xPos'] = 0
    phantom['yPos'] = 0
    phantom['zPos'] = 0
    phantom['xDir'] = 0
    phantom['yDir'] = 0
    phantom['zDir'] = 1
    phantom['zsize'] = 10
    phantom['radius'] = 10
    phantom['material'] = "air"
    thePhantoms.append(phantom)
    #Modify photons directly in data
    photons['killPhotons'] = False # true = Photon is stored in the stack when it is created, false = Same as true, but the photon propagates until it is absorbed. ntuple only stores the information at the time the photon is created in both cases. false may be useful if there is a detector at the end of the scintillating fibers, or to make the visualization more realistic.
    #Modify the detectors
    theDetectors = []
    zPos = [30]
    for z in zPos:
        theDet = copy.copy(detector)
        theDet['xPosDetector'] = 0.0
        theDet['yPosDetector'] = 0.0
        theDet['zPosDetector'] = z
        theDet['xDirDetector'] = 0.0
        theDet['xDirDetector'] = 0.0
        theDet['xPosDetector'] = 0.0
        theDet['xSizeDetector'] = 20.0
        theDet['ySizeDetector'] = 20.0
        theDet['zSizeDetector'] = 25.0
        theLayers = []
        zLayerPos = [10.5, 9.5, -9.5, -10.5]
        zDirLayer = [90.0, 0.0, 90.0, 0.0]
        for i, ilayer in enumerate(zLayerPos):
            theLayer = copy.copy(layer)
            theLayer['xPosLayer'] = 0.0
            theLayer['yPosLayer'] = 0.0
            theLayer['zPosLayer'] = zLayerPos[i]
            theLayer['xDirLayer'] = 0.0
            theLayer['yDirLayer'] = 0.0
            theLayer['zDirLayer'] = zDirLayer[i]
            theLayer['xSizeLayer'] = 20.0
            theLayer['ySizeLayer'] = 20.0
            theLayer['zSizeLayer'] = 1.0
            theFibers = []
            NFibers = 1
            coreRad = 0.25 # cm; core radius
            cladRad = 0.35 # cm; cladding radius
            FiberPitch = 0.725 # cm; fiber outer diameter = 2 * (claddingRadius + jacket 0.01 cm)
            FiberZsize = 2 * (cladRad + 0.01)
            FiberXsize = 2 * (cladRad + 0.01)
            FiberYsize = theLayer['ySizeLayer']
            # Layers measuring the same axis (0-2 and 1-3) are offset by half a fiber pitch to avoid gaps in the coverage
            shift = 0.0
            for j in range(NFibers):
                xfiber = (j - (NFibers - 1) / 2.0) * FiberPitch + shift
                yfiber = 0.0
                zfiber = 0.0
                theFiber = copy.copy(fiber)
                theFiber['xPosSensor'] = xfiber
                theFiber['yPosSensor'] = yfiber
                theFiber['zPosSensor'] = zfiber
                theFiber['xDirSensor'] = 90
                theFiber['yDirSensor'] = 0
                theFiber['zDirSensor'] = 0
                theFiber['xSizeSensor'] = FiberXsize
                theFiber['ySizeSensor'] = FiberYsize
                theFiber['zSizeSensor'] = FiberZsize
                theFiber['coreRadius'] = coreRad
                theFiber['claddingRadius'] = cladRad
                theFiber['length'] = FiberYsize/2.0
                theFiber['coreMaterial'] = 'EJ204'
                theFiber['claddingMaterial'] = 'scint_clad'
                theFibers.append(theFiber)
            theLayer['Fibers'] = theFibers
            theLayers.append(theLayer)
        theDet['Layers'] = theLayers
        theDetectors.append(theDet)

    #Nanowire readout (photons at the fiber ends -> current in nA)
    nanowire = dict()
    nanowire['dt'] = 0.005          # ns, same as FIXED_DT of the network
    nanowire['tmax'] = 30.0         # ns, must be a multiple of dt
    nanowire['tauRise'] = 0.6       # ns
    nanowire['tauDecay'] = 1.0      # ns
    nanowire['responsivity'] = 0.01 # nA per photon
    nanowire['tOffset'] = 5.0       # ns, time of the first hit of the event inside the window
    nanowire['minPhotons'] = 1
    photons['storeExitPhotons'] = False   # False to save disk once validated
    photons['storePhotons'] = False

    newData = dict()
    newData['theWorld'] = theWorld
    newData['theBeam'] = theBeam
    newData['Photons'] = photons
    newData['Nanowire'] = nanowire
    newData['Phantoms'] = thePhantoms
    newData['Detectors'] = theDetectors

    print('Making a copy')
    json_str = json.dumps(newData, indent=4)
    with open(opts.outputFile, "w") as f:
        f.write(json_str)

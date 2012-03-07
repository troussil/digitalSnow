#include <sstream>
#include <iomanip>

/////////////////////
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

namespace po = boost::program_options;

/////////////////////
#include <DGtal/base/Common.h>
#include <DGtal/helpers/StdDefs.h>
#include "DGtal/io/readers/VolReader.h"

#include "DGtal/images/ImageContainerBySTLVector.h"
#include "DGtal/images/ImageContainerBySTLMap.h"
#include "DGtal/base/BasicFunctors.h"
#include "DGtal/images/ConstImageAdapter.h"

#include "DGtal/shapes/Shapes.h"

#include "LocalBalloonForce.h"
#include "LocalMCM.h"
#include "LocalMCMforDT.h"
#include "FrontierEvolver.h"


using namespace Z3i; 

/////////////////////////// useful functions
#include "deformationFunctions.h"
#include "deformationDisplay3d.h"



///////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{


  DGtal::trace.info() << "local evolution ";
  DGtal::trace.emphase() << "(version "<< DGTAL_VERSION << ")"<< std::endl;
  


  // parse command line ----------------------------------------------
  po::options_description general_opt("Allowed options are");
  general_opt.add_options()
    ("help,h", "display this message")
    ("inputImage,i",  po::value<string>(), "Binary image to initialize the starting interface (vol format)" )
    ("timeStep,t",  po::value<double>()->default_value(1.0), "Time step for the evolution" )
    ("displayStep,d",  po::value<int>()->default_value(1), "Number of time steps between 2 drawings" )
    ("stepsNumber,n",  po::value<int>()->default_value(1), "Maximal number of steps" )
    ("bandWidth,w",  po::value<double>()->default_value(1.0), "Width of the flipping band" )
    ("balloonForce,k",  po::value<double>()->default_value(0.0), "Balloon force" )
    ("outputFiles,o",   po::value<string>()->default_value("interface"), "Output files basename" )
    ("outputFormat,f",   po::value<string>()->default_value("png"), 
"Output files format: either <png> (3d to 2d, default) or <vol> (3d)" )
    ("withVisu", "Enables interactive 3d visualization before and after evolution" );

  
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, general_opt), vm);  
  po::notify(vm);    
  if(vm.count("help")||argc<=1)
    {
      trace.info()<< "Local deformation" << std::endl
      << "Basic usage: "<<std::endl
      << argv[0] << " [other options] -t <time step> --withVisu" << std::endl
      << general_opt << "\n";
      return 0;
    }
  
  //Parse options

  //time step
  double tstep; 
  if (!(vm.count("timeStep"))) trace.info() << "time step default value: 1.0" << std::endl; 
  tstep = vm["timeStep"].as<double>(); 
    
  //iterations
  int step; 
  if (!(vm.count("displayStep"))) 
    trace.info() << "number of steps between two drawings: 1 by default" << std::endl; 
  step = vm["displayStep"].as<int>(); 
  int max; 
  if (!(vm.count("stepsNumber"))) 
    trace.info() << "maximal number of steps: 1 by default" << std::endl; 
  max = vm["stepsNumber"].as<int>(); 


  //files
  std::string outputFiles; 
  if (!(vm.count("outputFiles"))) 
    trace.info() << "output files beginning with : interface" << std::endl;
  outputFiles = vm["outputFiles"].as<std::string>();

  //files format
  std::string format; 
  if (!(vm.count("outputFormat"))) 
    trace.info() << "output files format is png (3d to 2d) " << std::endl;
  format = vm["outputFormat"].as<std::string>();
  if ((format != "png")&&(format != "vol")) 
    {
    trace.info() << "format is expected to be either png or vol " << std::endl;
    return 0; 
    }


  //image of labels
  typedef ImageContainerBySTLVector<Domain,short int> LabelImage; 
  if (!(vm.count("inputImage"))) 
    {
    trace.info() << "you must use --inputImage option" << std::endl;
    return 0; 
    }
  string imageFileName = vm["inputImage"].as<std::string>();
  trace.emphase() << imageFileName <<std::endl; 
  LabelImage labelImage = VolReader<LabelImage>::importVol( imageFileName);
  inv(labelImage); 

  //3d to 2d display
  std::stringstream ss; 
  ss << outputFiles << "0001"; 
  writeImage( labelImage, ss.str(), format );

  //interactive display before the evolution
  if (vm.count("withVisu")) displayImage( argc, argv, labelImage ); 

  //balloon force
  double k = 0.0; 
  if (!(vm.count("balloonForce"))) trace.info() << "balloon force default value: 0" << std::endl; 
  k = vm["balloonForce"].as<double>(); 

  //width of the flipping band
  double w = 1.0; 
  if (!(vm.count("bandWidth"))) trace.info() << "band width default value: 1" << std::endl; 
  w = vm["bandWidth"].as<double>(); 
  if( (w < 0) || (w > 1) ) 
    {
      trace.info() << "The band width should be between 0 and 1 " << std::endl; 
      return 0; 
    }

  //algo
  //space
  KSpace ks;
  Domain d( labelImage.domain() ); 
  ks.init( d.lowerBound(), d.upperBound(), true ); 
 
  //distance map
  typedef ImageContainerBySTLMap<Domain,double> DistanceImage; 
  DistanceImage map( d, 0.0 );

  //data functions
  ImageContainerBySTLMap<Domain,double> g( d, 1.0 ); 
  //predicate and functor
  typedef TruePointPredicate<Point> Predicate; 
  Predicate predicate; 
  // typedef LocalBalloonForce<DistanceImage, 
  //  ImageContainerBySTLMap<Domain,double> > Functor; 
  // Functor functor(map, g, k); 
  typedef LocalMCM<DistanceImage, 
   ImageContainerBySTLMap<Domain,double> > Functor; 
  Functor functor(map, g, g); 
  // typedef LocalMCMforDT<DistanceImage> Functor; 
  // Functor functor(map); 

  //getting a bel
  Thresholder<LabelImage::Value> t( 0 ); 
  typedef ConstImageAdapter<LabelImage, Thresholder<LabelImage::Value>, bool> BinaryImage; 
  BinaryImage binaryImage(labelImage, t);
  try {
    KSpace::SCell bel = Surfaces<KSpace>::findABel( ks, binaryImage, 10000 );

    trace.info() << "starting bel: "
		 << bel
		 << std::endl;
 
    //frontier evolver
    FrontierEvolver<KSpace, LabelImage, DistanceImage, Functor, Predicate> 
      e(ks, labelImage, map, bel, functor, predicate, w ); 

    double sumt = 0; 
    for (unsigned int i = 1; i <= max; ++i) 
      {
	std::stringstream s0; 
	s0 << "iteration # " << i; 
	trace.beginBlock( s0.str() );

	//update
	sumt += e.update(); 

	if ((i%step)==0) 
	  {

	    //3d to 2d display
	    std::stringstream s; 
	    s << outputFiles << setfill('0') << std::setw(4) << (i/step)+1; 
	    writeImage( labelImage, s.str(), format );

	  }

	trace.info() << "Total time spent: " << sumt << std::endl; 
	trace.endBlock();   
      }


  } catch (DGtal::InputException i) {
    trace.emphase() << "starting bel not found" << std::endl; 
  }

  //interactive display after the evolution
  if (vm.count("withVisu")) displayImage( argc, argv, labelImage ); 

  
  return 0;
}


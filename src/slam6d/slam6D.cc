/**
 * @file
 * @brief Main programm for matching 3D scans (6D SLAM)
 *
 * Main programm to match 3D scans with ICP and the globally consistent LUM approach.
 * Use -i from the command line to match with ICP, and -I to match 3D Scans using the LUM algorithm.
 * 
 * @author Andreas Nuechter. Jacobs University Bremen gGmbH, Germany
 * @author Kai Lingemann. Institute of Computer Science, University of Osnabrueck, Germany.
 * @author Jochen Sprickerhof. Institute of Computer Science, University of Osnabrueck, Germany.
 */

#ifdef _MSC_VER
#ifdef OPENMP
#define _OPENMP
#endif
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

#define WANT_STREAM ///< define the WANT stream :)

#include <string>
using std::string;
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
#include <fstream>
using std::ifstream;

#include "slam6d/scan.h"

#include "slam6d/icp6Dapx.h"
#include "slam6d/icp6Dsvd.h"
#include "slam6d/icp6Dquat.h"
#include "slam6d/icp6Dortho.h"
#include "slam6d/icp6Dhelix.h"
#include "slam6d/icp6Ddual.h"
#include "slam6d/icp6Dlumeuler.h"
#include "slam6d/icp6Dlumquat.h"
#include "slam6d/icp6D.h"
#ifdef WITH_CUDA
#include "slam6d/cuda/icp6Dcuda.h"
#endif
#include "slam6d/lum6Deuler.h"
#include "slam6d/lum6Dquat.h"
#include "slam6d/ghelix6DQ2.h"
#include "slam6d/graphToro.h"
#include "slam6d/graphHOG-Man.h"
#include "slam6d/elch6Deuler.h"
#include "slam6d/elch6Dquat.h"
#include "slam6d/elch6DunitQuat.h"
#include "slam6d/elch6Dslerp.h"
#include "slam6d/loopToro.h"
#include "slam6d/loopHOG-Man.h"
#include "slam6d/graphSlam6D.h"
#include "slam6d/gapx6D.h"
#include "slam6d/graph.h"
#include "slam6d/globals.icc"

#ifndef _MSC_VER
#include <getopt.h>
#else
#include "XGetopt.h"
#endif

#include <csignal>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

//  Handling Segmentation faults and CTRL-C
void sigSEGVhandler (int v)
{
  static bool segfault = false;
  if(!segfault) {
    segfault = true;
    cout << endl
      << "# **************************** #" << endl
      << "  Segmentation fault or Crtl-C" << endl
      << "# **************************** #" << endl
      << endl
      << "Saving registration information in .frames files" << endl;
    for (unsigned int i = 0; i < Scan::allScans.size(); i++) {
      if (i % 2) cout << ".";
      cout.flush();
      delete Scan::allScans[i];
    }
    cout << endl;
  }
  exit(-1);
}


/**
 * Explains the usage of this program's command line parameters
 */
void usage(char* prog)
{
#ifndef _MSC_VER
  const string bold("\033[1m");
  const string normal("\033[m");
#else
  const string bold("");
  const string normal("");
#endif
  cout << endl
    << bold << "USAGE " << normal << endl
    << "   " << prog << " [options] directory" << endl << endl;
  cout << bold << "OPTIONS" << normal << endl

    << bold << "  -a" << normal << " NR, " << bold << "--algo=" << normal << "NR   [default: 1]" << endl
    << "         selects the minimizazion method for the ICP matching algorithm" << endl
    << "           1 = unit quaterion based method by Horn" << endl
    << "           2 = singular value decomposition by Arun et al. " << endl
    << "           3 = orhtonormal matrices by Horn et al." << endl
    << "           4 = dual quaternion method by Walker et al." << endl
    << "           5 = helix approximation by Hofer & Potmann" << endl
    << "           6 = small angle approximation" << endl
    << "           7 = Lu & Milios style, i.e., uncertainty based, with Euler angles" << endl
    << "           8 = Lu & Milios style, i.e., uncertainty based, with Quaternion" << endl
    << endl
    << bold << "  -A" << normal << " NR, " << bold << "--anim=" << normal << "NR   [default: first and last frame only]" << endl
    << "         if specified, use only every NR-th frame for animation" << endl
    << endl
    << bold << "  -c" << normal << " NR, " << bold << "--cldist=" << normal << "NR   [default: 500]" << endl
    << "         specifies the maximal distance for closed loops" << endl
    << endl
    << bold << "  -C" << normal << " NR, " << bold << "--clpairs=" << normal << "NR   [default: 6]" << endl
    << "         specifies the minimal number of points for an overlap. If not specified" << endl
    << "         cldist is used instead" << endl
    << endl
    << bold << "  --cache" << normal << endl
    << "         turns on cached k-d tree search" << endl
    << endl
    << bold << "  -d" << normal << " NR, " << bold << "--dist=" << normal << "NR   [default: 25]" << endl
    << "         sets the maximal point-to-point distance for matching with ICP to <NR> 'units'" << endl
    << "         (unit of scan data, e.g. cm)" << endl
    << endl
    << bold << "  -D" << normal << " NR, " << bold << "--distSLAM="
    << normal << "NR   [default: same value as -d option]" << endl
    << "         sets the maximal point-to-point distance for matching with SLAM to <NR> 'units'" << endl
    << "         (unit of scan data, e.g. cm)" << endl
    << endl
    << bold << "  --DlastSLAM" << normal << " NR  [default not set]" << endl
    << "         sets the maximal point-to-point distance for the final SLAM correction," << endl
    << "         if final SLAM is not required don't set it." << endl
    << endl
    << bold << "  -e" << normal << " NR, " << bold << "--end=" << normal << "NR" << endl
    << "         end after scan NR" << endl
    << endl
    << bold << "  --exportAllPoints" << normal << endl
    << "         writes all registered reduced points to the file points.pts before" << endl
    << "         slam6D terminated" << endl
    << endl
    << bold << "  --epsICP=" << normal << "NR   [default: 0.00001]" << endl
    << "         stop ICP iteration if difference is smaller than NR" << endl
    << endl
    << bold << "  --epsSLAM=" << normal << " NR   [default: 0.5]" << endl
    << "         stop SLAM iteration if average difference is smaller than NR" << endl
    << endl
    << bold << "  -f" << normal << " F, " << bold << "--format=" << normal << "F" << endl
    << "         using shared library F for input" << endl
    << "         (chose F from {uos, uos_map, uos_rgb, uos_frames, uos_map_frames, old, rts, rts_map, ifp, riegl_txt, riegl_rgb, riegl_bin, zahn, ply, wrl, xyz, zuf, iais, front, x3d, rxp, ais })" << endl
    << endl
    << bold << "  -G" << normal << " NR, " << bold << "--graphSlam6DAlgo=" << normal << "NR   [default: 0]" << endl
    << "         selects the minimizazion method for the SLAM matching algorithm" << endl
    << "           0 = no global relaxation technique" << endl
    << "           1 = Lu & Milios extention using euler angles due to Borrmann et al." << endl
    << "           2 = Lu & Milios extention using using unit quaternions" << endl
    << "           3 = HELIX approximation by Hofer and Pottmann" << endl
    << "           4 = small angle approximation" << endl
    << "           5 = TORO" << endl
    << "           6 = HOG-Man" << endl
    << endl
    << bold << "  -i" << normal << " NR, " << bold << "--iter=" << normal << "NR [default: 50]" << endl
    << "         sets the maximal number of ICP iterations to <NR>" << endl
    << endl
    << bold << "  -I" << normal << " NR, " << bold << "--iterSLAM=" << normal << "NR [default: 0]" << endl
    << "         sets the maximal number of iterations for SLAM to <NR>" << endl
    << "         (if not set, graphSLAM is not executed)" << endl 
    << endl
    << bold << "  -l" << normal << " NR, " << bold << "--loopsize=" << normal << "NR [default: 20]" << endl
    << "         sets the size of a loop, i.e., a loop must exceed <NR> of scans" << endl
    << endl
    << bold << "  -L" << normal << " NR, " << bold << "--loop6DAlgo=" << normal << "NR   [default: 0]" << endl
    << "         selects the method for closing the loop explicitly" << endl
    << "           0 = no loop closing technique" << endl
    << "           1 = euler angles" << endl
    << "           2 = quaternions " << endl
    << "           3 = unit quaternions" << endl
    << "           4 = SLERP (recommended)" << endl
    << "           5 = TORO" << endl
    << "           6 = HOG-Man" << endl
    << endl
    << bold << "  --metascan" << normal << endl
    << "         Match current scan against a meta scan of all previous scans (default match against the last scan only)" << endl
    << endl
    << bold << "  -m" << normal << " NR, " << bold << "--max=" << normal << "NR" << endl
    << "         neglegt all data points with a distance larger than NR 'units'" << endl
    << endl
    << bold << "  -M" << normal << " NR, " << bold << "--min=" << normal << "NR" << endl
    << "         neglegt all data points with a distance smaller than NR 'units'" << endl
    << endl
    << bold << "  -n" << normal << " FILE, " << bold << "--net=" << normal << "FILE" << endl
    << "         specifies the file that includes the net structure for SLAM" << endl
    << endl
    << bold << "  -O" << normal << "NR (optional), " << bold << "--octree=" << normal << "NR (optional)" << endl
    << "         use randomized octree based point reduction (pts per voxel=<NR>)" << endl
    << "         requires " << bold << "-r" << normal <<" or " << bold << "--reduce" << endl
    << endl
    << bold << "  -p, --trustpose" << normal << endl
    << "         Trust the pose file, do not extrapolate the last transformation." << endl
    << "         (just for testing purposes, or gps input.)" << endl
    << endl
    << bold << "  -q, --quiet" << normal << endl
    << "         Quiet mode. Suppress (most) messages" << endl
    << endl
    << bold << "  -Q, --veryquiet" << normal << endl
    << "         Very quiet mode. Suppress all messages, except in case of error." << endl
    << endl
    << bold << "  -r" << normal << " NR, " << bold << "--reduce=" << normal << "NR" << endl
    << "         turns on octree based point reduction (voxel size=<NR>)" << endl
    << endl
    << bold << "  -R" << normal << " NR, " << bold << "--random=" << normal << "NR" << endl
    << "         turns on randomized reduction, using about every <NR>-th point only" << endl
    << endl
    << bold << "  -s" << normal << " NR, " << bold << "--start=" << normal << "NR" << endl
    << "         start at scan NR (i.e., neglects the first NR scans)" << endl
    << "         [ATTENTION: counting naturally starts with 0]" << endl
    << endl
    << bold << "  -u" << normal <<", "<< bold<<"--cuda" << normal << endl
    << "         this option activates icp running on GPU instead of CPU"<<endl
    << endl << endl;

  cout << bold << "EXAMPLES " << normal << endl
    << "   " << prog << " dat" << endl
    << "   " << prog << " --max=500 -r 10.2 -i 20 dat" << endl
    << "   " << prog << " -s 2 -e 10 dat" << endl << endl;
  exit(1);
}


/** A function that parses the command-line arguments and sets the respective flags.
 * @param argc the number of arguments
 * @param argv the arguments
 * @param dir the directory
 * @param red using point reduction?
 * @param rand use randomized point reduction?
 * @param mdm maximal distance match
 * @param mdml maximal distance match for SLAM
 * @param mni maximal number of iterations
 * @param start starting at scan number 'start'
 * @param end stopping at scan number 'end'
 * @param maxDist - maximal distance of points being loaded
 * @param minDist - minimal distance of points being loaded
 * @param quiet switches on/off the quiet mode
 * @param veryQuiet switches on/off the 'very quiet' mode
 * @param extrapolate_pose - i.e., extrapolating the odometry by the last transformation
 *        (vs. taking the pose file as <b>exact</b>)
 * @param meta match against all scans (= meta scan), or against the last scan only???
 * @param anim selects the rotation representation for the matching algorithm
 * @param mni_lum sets the maximal number of iterations for SLAM
 * @param net specifies the file that includes the net structure for SLAM
 * @param cldist specifies the maximal distance for closed loops
 * @param epsilonICP stop ICP iteration if difference is smaller than this value
 * @param epsilonSLAM stop SLAM iteration if average difference is smaller than this value
 * @param algo specfies the used algorithm for rotation computation
 * @param lum6DAlgo specifies the used algorithm for global SLAM correction
 * @param loopsize defines the minimal loop size
 * @return 0, if the parsing was successful. 1 otherwise
 */
int parseArgs(int argc, char **argv, string &dir, double &red, int &rand,
    double &mdm, double &mdml, double &mdmll,
    int &mni, int &start, int &end, int &maxDist, int &minDist, bool &quiet, bool &veryQuiet,
    bool &extrapolate_pose, bool &meta, int &algo, int &loopSlam6DAlgo, int &lum6DAlgo, int &anim,
    int &mni_lum, string &net, double &cldist, int &clpairs, int &loopsize,
    double &epsilonICP, double &epsilonSLAM, bool &use_cache, bool &exportPts, double &distLoop,
    int &iterLoop, double &graphDist, int &octree, bool &cuda_enabled, reader_type &type)
{
  int  c;
  // from unistd.h:
  extern char *optarg;
  extern int optind;

  /* options descriptor */
  // 0: no arguments, 1: required argument, 2: optional argument
  static struct option longopts[] = {
    { "format",          required_argument,   0,  'f' },  
    { "algo",            required_argument,   0,  'a' },
    { "loop6DAlgo",      required_argument,   0,  'L' },
    { "graphSlam6DAlgo", required_argument,   0,  'G' },
    { "net",             required_argument,   0,  'n' },
    { "iter",            required_argument,   0,  'i' },
    { "iterSLAM",        required_argument,   0,  'I' },
    { "max",             required_argument,   0,  'm' },
    { "loopsize",        required_argument,   0,  'l' },
    { "cldist",          required_argument,   0,  'c' },
    { "clpairs",         required_argument,   0,  'C' },
    { "min",             required_argument,   0,  'M' },
    { "dist",            required_argument,   0,  'd' },
    { "distSLAM",        required_argument,   0,  'D' },
    { "start",           required_argument,   0,  's' },
    { "end",             required_argument,   0,  'e' },
    { "reduce",          required_argument,   0,  'r' },
    { "octree",          optional_argument,   0,  'O' },
    { "random",          required_argument,   0,  'R' },
    { "quiet",           no_argument,         0,  'q' },
    { "veryquiet",       no_argument,         0,  'Q' },
    { "trustpose",       no_argument,         0,  'p' },
    { "anim",            required_argument,   0,  'A' },
    { "metascan",        no_argument,         0,  '2' }, // use the long format only
    { "DlastSLAM",       required_argument,   0,  '4' }, // use the long format only
    { "epsICP",          required_argument,   0,  '5' }, // use the long format only
    { "epsSLAM",         required_argument,   0,  '6' }, // use the long format only
    { "cache",           no_argument,         0,  '7' },
    { "exportAllPoints", no_argument,         0,  '8' },
    { "distLoop",        required_argument,   0,  '9' }, // use the long format only
    { "iterLoop",        required_argument,   0,  '1' }, // use the long format only
    { "graphDist",       required_argument,   0,  '3' }, // use the long format only
    { "cuda",            no_argument,         0,  'u' }, // cuda will be enabled
    { 0,           0,   0,   0}                    // needed, cf. getopt.h
  };

  cout << endl;
  while ((c = getopt_long(argc, argv, "O::f:A:G:L:a:r:R:d:D:i:l:I:c:C:n:s:e:m:M:uqQp", longopts, NULL)) != -1)
    switch (c)
    {
      case 'a':
        algo = atoi(optarg);
        if ((algo < 0) || (algo > 8)) {
          cerr << "Error: ICP Algorithm not available." << endl;
          exit(1);
        }	   
        break;
      case 'L':
        loopSlam6DAlgo = atoi(optarg);
        if (loopSlam6DAlgo < 0 || loopSlam6DAlgo > 6) {
          cerr << "Error: global loop closing algorithm not available." << endl;
          exit(1);
        }
        break;
      case 'G':
        lum6DAlgo = atoi(optarg);
        if ((lum6DAlgo < 0) || (lum6DAlgo > 6)) {
          cerr << "Error: global relaxation algorithm not available." << endl;
          exit(1);
        }
        break;
      case 'c':
        cldist = atof(optarg);
        break;
      case 'C':
        clpairs = atoi(optarg);
        break;
      case 'l':
        loopsize = atoi(optarg);
        break;
      case 'r':
        red = atof(optarg);
        break;
      case 'O':
        if (optarg) {
          octree = atoi(optarg);
        } else {
          octree = 1;
        }
        break;
      case 'R':
        rand = atoi(optarg);
        break;
      case 'd':
        mdm = atof(optarg);
        break;
      case 'D':
        mdml = atof(optarg);
        break;
      case 'i':
        mni = atoi(optarg);
        break;
      case 'I':
        mni_lum = atoi(optarg);
        break;
      case 'n':
        net = optarg;
        break;
      case 's':
        start = atoi(optarg);
        if (start < 0) { cerr << "Error: Cannot start at a negative scan number.\n"; exit(1); }
        break;
      case 'e':
        end = atoi(optarg);
        if (end < 0)     { cerr << "Error: Cannot end at a negative scan number.\n"; exit(1); }
        if (end < start) { cerr << "Error: <end> cannot be smaller than <start>.\n"; exit(1); }
        break;
      case 'm':
        maxDist = atoi(optarg);
        break;
      case 'M':
        minDist = atoi(optarg);
        break;
      case 'q':
        quiet = true;
        break;
      case 'Q':
        quiet = veryQuiet = true;
        break;
      case 'p':
        extrapolate_pose = false;
        break;
      case 'A':
        anim = atoi(optarg);
        break;
      case '2':  // = --metascan
        meta = true;
        break;
      case '4':  // = --DlastSLAM
        mdmll = atof(optarg);
        break;
      case '5':  // = --epsICP
        epsilonICP = atof(optarg);
        break;
      case '6':  // = --epsSLAM
        epsilonSLAM = atof(optarg);
        break;
      case '7':  // = --cache
        use_cache = true;
        break;
      case '8':  // not used
        exportPts = true;
        break;
      case '9':  // = --distLoop
        distLoop = atof(optarg);
        break;
      case '1':  // = --iterLoop
        iterLoop = atoi(optarg);
        break;
      case '3':  // = --graphDist
        graphDist = atof(optarg);
        break;
      case 'f': 
        if (strcasecmp(optarg, "uos") == 0) type = UOS;
        else if (strcasecmp(optarg, "uos_map") == 0) type = UOS_MAP;
        else if (strcasecmp(optarg, "uos_frames") == 0) type = UOS_FRAMES;
        else if (strcasecmp(optarg, "uos_map_frames") == 0) type = UOS_MAP_FRAMES;
        else if (strcasecmp(optarg, "uos_rgb") == 0) type = UOS_RGB;
        else if (strcasecmp(optarg, "old") == 0) type = OLD;
        else if (strcasecmp(optarg, "rts") == 0) type = RTS;
        else if (strcasecmp(optarg, "rts_map") == 0) type = RTS_MAP;
        else if (strcasecmp(optarg, "ifp") == 0) type = IFP;
        else if (strcasecmp(optarg, "riegl_txt") == 0) type = RIEGL_TXT;
        else if (strcasecmp(optarg, "riegl_rgb") == 0) type = RIEGL_RGB;
        else if (strcasecmp(optarg, "riegl_bin") == 0) type = RIEGL_BIN;
        else if (strcasecmp(optarg, "zahn") == 0) type = ZAHN;
        else if (strcasecmp(optarg, "ply") == 0) type = PLY;
        else if (strcasecmp(optarg, "wrl") == 0) type = WRL;
        else if (strcasecmp(optarg, "xyz") == 0) type = XYZ;
        else if (strcasecmp(optarg, "zuf") == 0) type = ZUF;
        else if (strcasecmp(optarg, "asc") == 0) type = ASC;
        else if (strcasecmp(optarg, "iais") == 0) type = IAIS;
        else if (strcasecmp(optarg, "front") == 0) type = FRONT;
        else if (strcasecmp(optarg, "x3d") == 0) type = X3D;
        else if (strcasecmp(optarg, "rxp") == 0) type = RXP;
        else if (strcasecmp(optarg, "ais") == 0) type = AIS;
        else {
          abort ();
        }
        break;
      case 'u':
        cuda_enabled = true;
        break;
      case '?':
        usage(argv[0]);
        return 1;
      default:
        abort ();
    }

  if (optind != argc-1) {
    cerr << "\n*** Directory missing ***" << endl;
    usage(argv[0]);
  }
  dir = argv[optind];

#ifndef _MSC_VER
  if (dir[dir.length()-1] != '/') dir = dir + "/";
#else
  if (dir[dir.length()-1] != '\\') dir = dir + "\\";
#endif

  return 0;
}

/**
 * This function is does all the matching stuff
 * it iterates over all scans using the algorithm objects to calculate new poses
 * objects could be NULL if algorithm should not be used
 *
 * @param cldist maximal distance for closing loops
 * @param loopsize minimal loop size
 * @param allScans Contains all laser scans
 * @param my_icp6D the ICP implementation
 * @param meta_icp math ICP against a metascan
 * @param use_cache Indicates if cached versions of the search tree has to be build
 * @param my_loopSlam6D used loopoptimizer
 * @param my_graphSlam6D used global optimization
 * @param nrIt The number of iterations the global SLAM-algorithm will run
 * @param epsilonSLAM epsilon for global SLAM iteration
 * @param mdml maximal distance match for global SLAM
 * @param mdmll maximal distance match for global SLAM after all scans ar matched
 */
void matchGraph6Dautomatic(double cldist, int loopsize, vector <Scan *> allScans, icp6D *my_icp6D,
					  bool meta_icp, bool use_cache, bool cuda_enabled,
					  loopSlam6D *my_loopSlam6D, graphSlam6D *my_graphSlam6D, int nrIt,
					  double epsilonSLAM, double mdml, double mdmll, double graphDist,
					  bool &eP, reader_type type)
{
  double cldist2 = sqr(cldist);

  // list of scan for metascan
  vector < Scan* > metas;

  // graph for loop optimization
  graph_t g;

  int n = allScans.size();

  int loop_detection = 0;
  double dist, min_dist = -1;
  int first = 0, last = 0;

  for(int i = 1; i < n; i++) {
    cout << i << "/" << n << endl;

    add_edge(i-1, i, g);

    if(eP) {
      allScans[i]->mergeCoordinatesWithRoboterPosition(allScans[i-1]);
    }

    //Hack to get all icp transformations into the .frames Files
    if(i == n-1 && my_icp6D != NULL && my_icp6D->get_anim() == -2) {
      my_icp6D->set_anim(-1);
    }

    /*if(i == 85 || i == 321 || i == 533) {
      my_icp6D->set_anim(1);
      }*/

    if(my_icp6D != NULL){
      cout << "ICP" << endl;
      // Matching strongly linked scans with ICPs
      if(meta_icp) {
        metas.push_back(allScans[i - 1]);
        Scan *meta_scan = new Scan(metas, use_cache, cuda_enabled);
        my_icp6D->match(meta_scan, allScans[i]);
        delete meta_scan;
      } else {
        switch(type) {
          case UOS_MAP:
          case UOS_MAP_FRAMES:
            my_icp6D->match(allScans[0], allScans[i]);
            break;
          case RTS_MAP:
            //untested (and could not work)
            //if(i < 220-22 && i > 250-22) match(allScans[0], CurrentScan);
            my_icp6D->match(allScans[0], allScans[i]);
            break;
          default:
            my_icp6D->match(allScans[i - 1], allScans[i]);
            break;
        }
      }
    } else {
      double id[16];
      M4identity(id);
      allScans[i]->transform(id, Scan::ICP, 0);
    }

    /*if(i == 85 || i == 321 || i == 533) {
      my_icp6D->set_anim(-2);
      }*/

    if(loop_detection == 1) {
      loop_detection = 2;
    }

    for(int j = 0; j < i - loopsize; j++) {
      dist = Dist2(allScans[j]->get_rPos(), allScans[i]->get_rPos());
      if(dist < cldist2) {
        loop_detection = 1;
        if(min_dist < 0 || dist < min_dist) {
          min_dist = dist;
          first = j;
          last = i;
        }
      }
    }

    if(loop_detection == 2) {
      loop_detection = 0;
      min_dist = -1;

      if(my_loopSlam6D != NULL) {
        cout << "Loop close: " << first << " " << last << endl;
        my_loopSlam6D->close_loop(allScans, first, last, g);
        add_edge(first, last, g);
      }

      if(my_graphSlam6D != NULL && mdml > 0) {
        int j = 0;
        double ret;
        do {
          // recalculate graph
          Graph *gr = new Graph(i + 1, cldist2, loopsize);
          cout << "Global: " << j << endl;
          ret = my_graphSlam6D->doGraphSlam6D(*gr, allScans, 1);
          delete gr;
          j++;
        } while (j < nrIt && ret > epsilonSLAM);
      }
    }
  }

  if(loop_detection == 1 && my_loopSlam6D != NULL) {
    cout << "Loop close: " << first << " " << last << endl;
    my_loopSlam6D->close_loop(allScans, first, last, g);
    add_edge(first, last, g);
  }

  if(my_graphSlam6D != NULL && mdml > 0.0) {
    int j = 0;
    double ret;
    do {
      // recalculate graph
      Graph *gr = new Graph(n, cldist2, loopsize);
      cout << "Global: " << j << endl;
      ret = my_graphSlam6D->doGraphSlam6D(*gr, allScans, 1);
      delete gr;
      j++;
    } while (j < nrIt && ret > epsilonSLAM);
  }

  if(my_graphSlam6D != NULL && mdmll > 0.0) {
    my_graphSlam6D->set_mdmll(mdmll);
    int j = 0;
    double ret;
    do {
      // recalculate graph
      Graph *gr = new Graph(n, sqr(graphDist), loopsize);
      cout << "Global: " << j << endl;
      ret = my_graphSlam6D->doGraphSlam6D(*gr, allScans, 1);
      delete gr;
      j++;
    } while (j < nrIt && ret > epsilonSLAM);
  }
}

/**
 * Main program for 6D SLAM.
 * Usage: bin/slam6D 'dir',
 * with 'dir' the directory of a set of scans
 * ...
 */
int main(int argc, char **argv)
{
  signal (SIGSEGV, sigSEGVhandler);
  signal (SIGINT,  sigSEGVhandler);

  cout << "slam6D - A highly efficient SLAM implementation based on scan matching" << endl
    << "         with 6 degrees of freedom" << endl
    << "(c) University of Osnabrueck, Germany, 2006 - 2009" << endl << endl;

  if (argc <= 1) {
    usage(argv[0]);
  }

  // parsing the command line parameters
  // init, default values if not specified
  string dir;
  double red   = -1.0, mdmll = -1.0, mdml = 25.0, mdm = 25.0;
  int    rand  = -1,   mni = 50;
  int    start = 0,   end = -1;
  bool   quiet      = false;
  bool   veryQuiet  = false;
  int    maxDist    = -1;
  int    minDist    = -1;
  bool   eP         = true;  // should we extrapolate the pose??
  bool   meta       = false;  // match against meta scan, or against LAST scan only?
  int    algo       = 1;
  int    mni_lum    = -1;
  double cldist     = 500;
  int    clpairs    = -1;
  int    loopsize   = 20;
  string net        = "none";
  int    anim       = -1;
  double epsilonICP = 0.00001;
  double epsilonSLAM = 0.5;
  bool   use_cache  = false;
  bool   exportPts  = false;
  int    loopSlam6DAlgo  = 0;
  int    lum6DAlgo  = 0;
  double distLoop   = 700.0;
  int iterLoop      = 100;
  double graphDist  = cldist;
  int octree       = 0;  // employ randomized octree reduction?
  bool cuda_enabled    = false;
  reader_type type    = UOS;

  parseArgs(argc, argv, dir, red, rand, mdm, mdml, mdmll, mni, start, end,
      maxDist, minDist, quiet, veryQuiet, eP, meta, algo, loopSlam6DAlgo, lum6DAlgo, anim,
      mni_lum, net, cldist, clpairs, loopsize, epsilonICP, epsilonSLAM,
      use_cache, exportPts, distLoop, iterLoop, graphDist, octree, cuda_enabled, type);

  cout << "slam6D will proceed with the following parameters:" << endl;
  //@@@ to do :-)

  // Get Scans
  Scan::readScans(type, start, end, dir, maxDist, minDist, true);

  int end_reduction = (int)Scan::allScans.size();
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (int iterator = 0; iterator < end_reduction; iterator++) {
    if (red > 0) {
      cout << "Reducing Scan No. " << iterator << endl;
    } else {
      cout << "Copying Scan No. " << iterator << endl;
    }
    // reduction filter for current scan!
    Scan::allScans[iterator]->calcReducedPoints(red, octree);
  }

  Scan::createTrees(use_cache, cuda_enabled);

  // at this point the points vector can not be used anymore!!!
  for (int iterator = 0; iterator < end_reduction; iterator++) {
    Scan::allScans[iterator]->clearPoints();
  }

  icp6Dminimizer *my_icp6Dminimizer = 0;
  switch (algo) {
    case 1 :
      my_icp6Dminimizer = new icp6D_QUAT(quiet);
      break;
    case 2 :
      my_icp6Dminimizer = new icp6D_SVD(quiet);
      break;
    case 3 :
      my_icp6Dminimizer = new icp6D_ORTHO(quiet);
      break;
    case 4 :
      my_icp6Dminimizer = new icp6D_DUAL(quiet);
      break;
    case 5 :
      my_icp6Dminimizer = new icp6D_HELIX(quiet);
      break;
    case 6 :
      my_icp6Dminimizer = new icp6D_APX(quiet);
      break;
    case 7 :
      my_icp6Dminimizer = new icp6D_LUMEULER(quiet);
      break;
    case 8 :
      my_icp6Dminimizer = new icp6D_LUMQUAT(quiet);
      break;
  }

  // match the scans and print the time used
  long starttime = GetCurrentTimeInMilliSec();
  if (mni_lum == -1 && loopSlam6DAlgo == 0) {
    icp6D *my_icp = 0;
    if (cuda_enabled) {
#ifdef WITH_CUDA	 
      my_icp = new icp6Dcuda(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
					    anim, epsilonICP, use_cache, cuda_enabled);
#else
      cout << "slam6d was not compiled for excuting CUDA code" << endl;
#endif	 
    } else {
      my_icp = new icp6D(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
					anim, epsilonICP, use_cache, cuda_enabled);
    }
    if (my_icp) my_icp->doICP(Scan::allScans);
    delete my_icp;
  } else if (clpairs > -1) {
    //!!!!!!!!!!!!!!!!!!!!!!!!
    icp6D *my_icp = 0;
    if (cuda_enabled) {
#ifdef WITH_CUDA	 
      my_icp = new icp6Dcuda(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
					    anim, epsilonICP, use_cache, cuda_enabled);
#else
      cout << "slam6d was not compiled for excuting CUDA code" << endl;
#endif	 
    } else {
      my_icp = new icp6D(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
					anim, epsilonICP, use_cache, cuda_enabled);
    }
    my_icp->doICP(Scan::allScans);
    graphSlam6D *my_graphSlam6D = new lum6DEuler(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta,
        rand, eP, anim, epsilonICP, use_cache, epsilonSLAM);
    my_graphSlam6D->matchGraph6Dautomatic(Scan::allScans, mni_lum, clpairs, loopsize);
    //!!!!!!!!!!!!!!!!!!!!!!!!		  
  } else {
    graphSlam6D *my_graphSlam6D = 0;
    switch (lum6DAlgo) {
      case 1 :
        my_graphSlam6D = new lum6DEuler(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta, rand, eP,
            anim, epsilonICP, use_cache, epsilonSLAM);
        break;
      case 2 :
        my_graphSlam6D = new lum6DQuat(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta, rand, eP,
            anim, epsilonICP, use_cache, epsilonSLAM);
        break;
      case 3 : 
        my_graphSlam6D = new ghelix6DQ2(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta, rand, eP,
            anim, epsilonICP, use_cache, epsilonSLAM);
        break;
      case 4 :
        my_graphSlam6D = new gapx6D(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta, rand, eP,
            anim, epsilonICP, use_cache, epsilonSLAM);
        break;
      case 5 :
        my_graphSlam6D = new graphToro(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta, rand, eP,
            -2, epsilonICP, use_cache, epsilonSLAM);
        break;
      case 6 :
        my_graphSlam6D = new graphHOGMan(my_icp6Dminimizer, mdm, mdml, mni, quiet, meta, rand, eP,
            -2, epsilonICP, use_cache, epsilonSLAM);
        break;
    }
    // Construct Network
    if (net != "none") {
      icp6D *my_icp = 0;
      if (cuda_enabled) {
#ifdef WITH_CUDA	 
        my_icp = new icp6Dcuda(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
            anim, epsilonICP, use_cache);
#else
        cout << "slam6d was not compiled for excuting CUDA code" << endl;
#endif	 
      } else {
        my_icp = new icp6D(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
            anim, epsilonICP, use_cache);
      }
      my_icp->doICP(Scan::allScans);

      Graph* structure;
      structure = new Graph(net);
      my_graphSlam6D->doGraphSlam6D(*structure, Scan::allScans, mni_lum);
      if(mdmll > 0.0) {
        my_graphSlam6D->set_mdmll(mdmll);
        my_graphSlam6D->doGraphSlam6D(*structure, Scan::allScans, mni_lum);
      }

    } else {
      icp6D *my_icp = 0;
      if(algo > 0) {
        if (cuda_enabled) {
#ifdef WITH_CUDA	 
          my_icp = new icp6Dcuda(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
              anim, epsilonICP, use_cache);
#else
          cout << "slam6d was not compiled for excuting CUDA code" << endl;
#endif	 
        } else {
          my_icp = new icp6D(my_icp6Dminimizer, mdm, mni, quiet, meta, rand, eP,
              anim, epsilonICP, use_cache);
        }

        loopSlam6D *my_loopSlam6D = 0;
        switch(loopSlam6DAlgo) {
          case 1:
            my_loopSlam6D = new elch6Deuler(veryQuiet, my_icp6Dminimizer, distLoop, iterLoop, rand, eP, 10, epsilonICP, use_cache);
            break;
          case 2:
            my_loopSlam6D = new elch6Dquat(veryQuiet, my_icp6Dminimizer, distLoop, iterLoop, rand, eP, 10, epsilonICP, use_cache);
            break;
          case 3:
            my_loopSlam6D = new elch6DunitQuat(veryQuiet, my_icp6Dminimizer, distLoop, iterLoop, rand, eP, 10, epsilonICP, use_cache);
            break;
          case 4:
            my_loopSlam6D = new elch6Dslerp(veryQuiet, my_icp6Dminimizer, distLoop, iterLoop, rand, eP, 10, epsilonICP, use_cache);
            break;
          case 5:
            my_loopSlam6D = new loopToro(veryQuiet, my_icp6Dminimizer, distLoop, iterLoop, rand, eP, 10, epsilonICP, use_cache);
            break;
          case 6:
            my_loopSlam6D = new loopHOGMan(veryQuiet, my_icp6Dminimizer, distLoop, iterLoop, rand, eP, 10, epsilonICP, use_cache);
            break;
        }

        matchGraph6Dautomatic(cldist, loopsize, Scan::allScans, my_icp, meta,
						use_cache, cuda_enabled, my_loopSlam6D, my_graphSlam6D,
						mni_lum, epsilonSLAM, mdml, mdmll, graphDist, eP, type);
        delete my_icp;
        if(loopSlam6DAlgo > 0) {
          delete my_loopSlam6D;
        }
      }
      if(my_graphSlam6D > 0) {
        delete my_graphSlam6D;
      }
    }
  }

  long endtime = GetCurrentTimeInMilliSec() - starttime;
  cout << "Matching done in " << endtime << " milliseconds!!!" << endl;

  if (exportPts) {
    cout << "Export all 3D Points to file \"points.pts\"" << endl;
    ofstream redptsout("points.pts");
    for(unsigned int i = 0; i < Scan::allScans.size(); i++) {
      for (int j = 0; j < Scan::allScans[i]->get_points_red_size(); j++) {
        redptsout << Scan::allScans[i]->get_points_red()[j][0] << " "
          << Scan::allScans[i]->get_points_red()[j][1] << " "
          << Scan::allScans[i]->get_points_red()[j][2] << endl;
      }
    }
    redptsout.close();
    redptsout.clear();
  }

  cout << "Saving registration information in .frames files" << endl;
  vector <Scan*>::iterator Iter = Scan::allScans.begin();
  for( ; Iter != Scan::allScans.end(); ) {
    Iter = Scan::allScans.begin();
    delete (*Iter);
    cout << ".";
    cout.flush(); 
  }
  Scan::allScans.clear();

  delete my_icp6Dminimizer;

  cout << endl << endl;
  cout << "Normal program end." << endl
    << (red < 0 && rand < 0 ? "(-> HINT: For a significant speedup, please use the '-r' or '-R' parameter <-)\n"
        : "")
    << endl;
}

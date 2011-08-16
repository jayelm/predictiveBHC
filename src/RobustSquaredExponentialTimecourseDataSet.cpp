#include <limits>
#include "RobustSquaredExponentialTimecourseDataSet.h"
#include "BlockCovarianceMatrix.h"

//CONSTRUCTORS
RobustSquaredExponentialTimecourseDataSet::RobustSquaredExponentialTimecourseDataSet() {}

RobustSquaredExponentialTimecourseDataSet::RobustSquaredExponentialTimecourseDataSet(string dataFile)
{
  //READ IN THE DATA FROM FILE
  ReadInData(dataFile);

}

RobustSquaredExponentialTimecourseDataSet::RobustSquaredExponentialTimecourseDataSet(const vector<vector<double> >& inputData)
{
  //DECLARATIONS
  //COPY THE DATA INTO THE OBJECT
  data = inputData;

  //cout << "Need to deal with the time points here test!" << endl;

  //FIND THE DATA SIZE
  nDataItems  = data.size();
  nFeatures   = data[0].size();
  nTimePoints = nFeatures;

  //PRINT OUT SOME USEFUL INFORMATION
  //cout << "----------" << endl;
  //cout << "nTimeSeries: " << nDataItems << endl;
  //cout << "nTimePoints: " << nTimePoints << endl;
  //cout << "----------" << endl;
}



// COMPUTE THE LOG-EVIDENCE FOR A SINGLE CLUSTER CONTAINING THE DATA ITEMS IDENTIFIED BY 'itemIndex'
//for now, we just find optimised hyperparameters here; in general, we could consider marginalising over them.
//If we're optimising the hyperparameters, do we care about storing the best-fit values?  (knowing about the noise level, for
//example, might be interesting).  If so, we'll need a way of returning the hyperparameters to R.
//Perhaps the Node class needs the capacity to store the best-fit hyperparameters for the mixture component it represents??
double RobustSquaredExponentialTimecourseDataSet::SingleClusterLogEvidence(const vector<int>& itemIndex, double& lengthScale, double& noiseFreeScale, double& noiseSigma, double& mixtureComponent)
{
  //DECLARATIONS
  int i, j;
  int index, counter=0;
  int nCurrentItems=itemIndex.size();
  double logEvidence=-numeric_limits<double>::infinity(),
    replicateNoise;
  vector<double> extractedData, yValues;


  //EXTRACT THE DATA POINTS FOR THIS CURRENT CLUSTER
  //data vector is nDataItems*nTimePoints
  for (i=0; i<nCurrentItems; i++)
  {
    for (j=0; j<nTimePoints; j++)
    {
      index = itemIndex[i];
      extractedData.push_back(data[index][j]);//store the corresponding data value
    }
  }
  //RE-CONSTRUCT THE DATA VECTOR SO IT HAS THE CORRECT ORDERING FOR THE BlockCovarianceMatrix FORM WE'LL BE USING
  yValues = extractedData;
  counter = 0;
  for (i=0; i<nCurrentItems; i++)
  {
    for (j=0; j<nTimePoints; j++)
    {
      index          = j*nCurrentItems + i;
      yValues[index] = extractedData[counter];
      counter++;
    }
  }

  //OPTIMISE THE HYPERPARAMETERS (LENGTH SCALE); RETURN THE OPTIMISED LOG-EVIDENCE VALUE
  if (noise_mode ==0)
  {
    OptimiseHyperparameters(yValues, lengthScale, noiseFreeScale, noiseSigma);
    logEvidence = ComputeRobustLogEvidence(yValues, nCurrentItems, lengthScale, noiseFreeScale, noiseSigma, mixtureComponent);
  }
  else if (noise_mode == 2)
  {
    replicateNoise = GetClusterSEMS(itemIndex);
    OptimiseHyperparametersEstimatedNoise(yValues, lengthScale, noiseFreeScale, noiseSigma, replicateNoise);
    logEvidence = ComputeRobustLogEvidence(yValues, nCurrentItems, lengthScale, noiseFreeScale, noiseSigma, mixtureComponent);
  }
  
  return logEvidence;
}



//METHOD TO COMPUTE THE ROBUST LOGEVIDENCE MISSING OUT A SINGLE OBSERVATION EACH TIME
double RobustSquaredExponentialTimecourseDataSet::ComputeRobustLogEvidence(vector<double> yValues, int nCurrentItems, double& lengthScale, double& noiseFreeScale, double& noiseSigma, double& mixtureComponent)
{
  //DECLARATIONS
  int blockSize, k,i;
  double lpst, CF1=0, clpst, pst, st, lst, lft, logfirstEv, logsecondEv, logEvidence, detCovarFunctionk;
  long double C1=0, C2, a, atop, abot;
  vector<double> yValuesk, yValueskCopy;
  BlockCovarianceMatrix fullCovarFunction, covarFunctionk, invCovarFunctionk;

  //INITIALISATIONS
  st = 0;
  blockSize = yValues.size() / nTimePoints;
  //cout << "yValues.size():" << yValues.size() << "nTimePoints:" << nTimePoints << endl;
  //cout << "LS:" << *p_lengthScale << " NF:" << *p_NoiseFree << " N:" << *p_Noise << endl;

  //COMPUTE FIRST TERM
  fullCovarFunction = SquareExponentialCovarianceFunction(lengthScale, blockSize, noiseFreeScale);
  fullCovarFunction = AddNoiseToCovarianceFunction(fullCovarFunction, noiseSigma);
  lft = ComputeLogEvidence(fullCovarFunction, yValues);
  //cout << "lft:" << lft << endl;
  //cout << "fullCovarFunction.noiseFreeCoeff[0]:" << fullCovarFunction.noiseFreeCoeff[0][0] << " " << fullCovarFunction.noiseFreeCoeff[0][1]<< " " << fullCovarFunction.noiseFreeCoeff[0][2] << " " << fullCovarFunction.noiseFreeCoeff[0][3] << endl;
  //cout << "fullCovarFunction.noiseFreeCoeff[1]:" << fullCovarFunction.noiseFreeCoeff[1][0] << " " << fullCovarFunction.noiseFreeCoeff[1][1]<< " " << fullCovarFunction.noiseFreeCoeff[1][2] << " " << fullCovarFunction.noiseFreeCoeff[1][3]<< endl;
  //cout << "fullCovarFunction.noiseFreeCoeff[2]:" << fullCovarFunction.noiseFreeCoeff[2][0] << " " << fullCovarFunction.noiseFreeCoeff[2][1]<< " " << fullCovarFunction.noiseFreeCoeff[2][2] << " " << fullCovarFunction.noiseFreeCoeff[2][3]<< endl;
  //cout << "fullCovarFunction.noiseFreeCoeff[3]:" << fullCovarFunction.noiseFreeCoeff[3][0] << " " << fullCovarFunction.noiseFreeCoeff[3][1]<< " " << fullCovarFunction.noiseFreeCoeff[3][2] << " " << fullCovarFunction.noiseFreeCoeff[3][3]<< endl;
  //cout << "fullCovarFunction.noisyCoeff:" << fullCovarFunction.noisyCoeff[0] << " " << fullCovarFunction.noisyCoeff[1] << " " << fullCovarFunction.noisyCoeff[2] << " " << fullCovarFunction.noisyCoeff[3] << endl;
  //cout << "nCurrentItems:" << nCurrentItems << "nTimePoints:" << nTimePoints << endl;
  //cout << "yValues:" ;
  //for (i=0; i< nCurrentItems*nTimePoints; i++){
  //	cout << "y:" << i << " " << yValues[i] << endl;
  //}
  //COMPUTE SECOND TERM
  //miss out a timepoint each time
  for (k=0; k<nTimePoints; k++)
  {
    yValuesk = yValues;
    yValuesk.erase(yValuesk.begin() + (k*nCurrentItems), yValuesk.begin()+ (k*nCurrentItems +nCurrentItems)); //erase the obs values for time k from yValuesk
    yValuesk.insert(yValuesk.begin(), yValues.begin() +(k*nCurrentItems), yValues.begin() + (k*nCurrentItems +nCurrentItems)); //insert the obs values for time k at the beginning of yValuesk
    //Get covarmatrix when a single obs from timepoint k is missing
    covarFunctionk = CovarianceFunctionMissingSingleObservation(fullCovarFunction.noiseFreeCoeff, fullCovarFunction.noisyCoeff, fullCovarFunction.blockSize, fullCovarFunction.nRank, k);
    detCovarFunctionk = covarFunctionk.ComputeMatrixMissingSingleObservationLogDeterminant(); //Find the determinant of the covar matrix for a missing obs
    invCovarFunctionk = covarFunctionk; //make a copy of the covarFunctionk
    invCovarFunctionk.InvertMatrixMissingSingleObservation(); //invert the copy of covarFunctionk
    //for each obs for timepoint k, get the contribution to evidence
    for (i=0; i < nCurrentItems; i++)
    {
      yValueskCopy = yValuesk; //reset yValuesCopy to have all obs with time k at the top
      yValueskCopy.erase (yValueskCopy.begin()+i); //erase an observation

      lpst = ComputePartRobustLogEvidenceMissingSingleObservation(detCovarFunctionk, invCovarFunctionk, yValueskCopy); //Find the log evidence contribution by missing this observation
      //cout << "k:" << k << " i:" << i << " lpst:" << lpst << endl;

      if (k == 0 && i==0)
      {
        CF1 = lpst; //take the common factor to be the first term
      }
      clpst = lpst - CF1;  //corrected log partial second term
      pst = exp(clpst);
      st += pst; //add to the running sum
    }
  }
  lst = log(st);

  lst += CF1; //add back the common factor

  //cout << "lstbeforeRange:" << lst << "exp(lst)" << (long double) exp(lst) << endl;

  //Multiply the likelihood term arising from missing single observations by the outlier likelihood function: 1/dataRange
  lst += log(1/dataRange);

  //cout << "dataRange:" << dataRange << endl;

  //FIND THE OPTIMAL MIXING COMPONENTS

  C1 = (long double) exp(lft);
  C2 = (long double) exp(lst);
  atop = (long double) (nCurrentItems*nTimePoints-1)*C2;
  abot = (long double) nCurrentItems*nTimePoints *(C2-C1);

  a = atop / abot;
  //cout << "atop:" << atop << " " << "abot:" << abot << " " << "a:" << a << endl;
  //cout << "lft:" << " " << lft << "lst:" <<  " " << lst << "C1:" <<  " " << C1 << "C2:" << " " << C2 << endl;
  if ( 0.0 < a && a < 1.0)
  {
    mixtureComponent = a;
  }
  else mixtureComponent = 1.0;
  //cout << "interMixtureComponenet:" << mixtureComponent << endl;
  //cout << "mixture component:" << mixtureComponent << endl;

  //FINDING THE OVERALL LOG EVIDENCE
  if (0.0 < mixtureComponent && mixtureComponent < 1.0)
  {
    logfirstEv = nCurrentItems*nTimePoints*log(mixtureComponent) + lft;
    logsecondEv = ((nCurrentItems*nTimePoints)-1)*log(mixtureComponent) + log(1-mixtureComponent) + lst;

    //cout << "logfirstEv:" << logfirstEv << " logsecondEv:" << logsecondEv << endl;
    logEvidence = log(exp(logfirstEv - logfirstEv) + exp(logsecondEv - logfirstEv)) + logfirstEv;
  }
  else logEvidence = nCurrentItems*nTimePoints*log(mixtureComponent) + lft;

  //cout << "This is the end of the logEvidence:" << logEvidence << endl;
  return(logEvidence);
}

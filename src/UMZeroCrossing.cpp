#include "UMZeroCrossing.h"
//______________________________________________________________________________
UMZeroCrossing::UMZeroCrossing(){
   fUseTimeRange     = false;
   fUseMidpoint      = false;
   fUseLinearInterp  = false;
   fUseLeastSq       = false;
   fUseIntegerCycles = false; 
   fNPTS             = 1; 
   fNPTSUseable      = 0; 
   fStep             = 0; 
   fTMin             = 0;
   fTMax             = 0;
   fExpFreq          = 0; 
   fSampleFreq       = 0; 
   fVerbosity        = 0;
   fNZC              = 0;
   // fFileManager     = new UMFileManager(); 
   const int N = 3;
   fZC      = new int[N];
   fFREQ    = new double[N];
   fFREQ_ph = new double[N];
   fNC      = new double[N];
   for(int i=0;i<N;i++){
      fFREQ[i]    = 0;  
      fFREQ_ph[i] = 0;  
      fZC[i]      = 0;  
      fNC[i]      = 0; 
   } 
   const int NN = fNPTS;
   fX  = new double[NN]; 
   fY  = new double[NN]; 
   fEY = new double[NN];
   ClearAnaArrays();
   const int MAX = MAX_SIZE; 
   fNCrossing      = new int[MAX]; 
   fCrossingIndex  = new int[MAX]; 
   fTcross         = new double[MAX]; 
   fVcross         = new double[MAX];
   fFreqAtCrossing = new double[MAX];  
   fNumCycles      = new double[MAX];
   ClearVectors(); 
}
//______________________________________________________________________________
UMZeroCrossing::~UMZeroCrossing(){
   delete[] fX; 
   delete[] fY; 
   delete[] fEY;
   delete[] fFREQ;
   delete[] fFREQ_ph;
   delete[] fZC; 
   delete[] fNC;
   delete[] fNCrossing;
   delete[] fCrossingIndex;
   delete[] fTcross;
   delete[] fVcross;
   delete[] fFreqAtCrossing; 
   delete[] fNumCycles; 
   delete fFileManager; 
}
//______________________________________________________________________________
void UMZeroCrossing::UpdateParameters(){
   // Freq_exp   = expected frequency
   // SampleFreq = Frequency of ADC clock
   // step_size  = number of points to skip after finding a zero crossing
   // NPTS       = number of points to use in fitting for zero crossing 

   double T_exp  = 1./fExpFreq;
   double N_exp  = T_exp*fSampleFreq;       // number of points for one period 
   int step_size = (int)( (1./16.)*N_exp );  // skip 1/8 of a period 
   int NPTS      = step_size/2;             // use step_size/2 

   SetStepSize(step_size); 
   SetNumPointsForFits(NPTS); 

   if(fVerbosity>=3){ 
      printf("[UMZeroCrossing::UpdateParameters]: Expected Frequency: %.7lf \n",fExpFreq   );
      printf("[UMZeroCrossing::UpdateParameters]: Sampling Frequency: %.7lf \n",fSampleFreq);
      printf("[UMZeroCrossing::UpdateParameters]: Step size:          %d    \n",step_size  );
      printf("[UMZeroCrossing::UpdateParameters]: Number of points:   %d    \n",NPTS       );
   }

}
//______________________________________________________________________________
void UMZeroCrossing::SetNumPointsForFits(int n){
   fNPTS = n;
   InitAnaArrays();
}
//______________________________________________________________________________
void UMZeroCrossing::InitAnaArrays(){
   // analysis arrays: data points to use in finding a zero crossing
   delete[] fX;  
   delete[] fY;  
   delete[] fEY;  
   const int N = fNPTS;
   if(N>0){
      fX  = new double[N]; 
      fY  = new double[N]; 
      fEY = new double[N];
      ClearAnaArrays();  
   }else{
      std::cout << "[UMZeroCrossing::InitAnaArrays]: Cannot initialize arrays!  Is NPTS set?" << std::endl;
      exit(1);
   }
}
//______________________________________________________________________________
void UMZeroCrossing::Reset(){
   // resets all data members (useful for multiple zero crossing calculations) 
   ClearAnaArrays();
   ClearVectors();
}
//______________________________________________________________________________
void UMZeroCrossing::ClearAnaArrays(){
   for(int i=0;i<fNPTS;i++){
      fX[i]  = 0; 
      fY[i]  = 0; 
      fEY[i] = 0; 
   }
}
//______________________________________________________________________________
void UMZeroCrossing::ClearVectors(){
   for(int i=0;i<MAX_SIZE;i++){
      fNCrossing[i]      = 0;
      fCrossingIndex[i]  = 0; 
      fTcross[i]         = 0; 
      fVcross[i]         = 0;
      fFreqAtCrossing[i] = 0;  
      fNumCycles[i]      = 0; 
   }
}
//______________________________________________________________________________
void UMZeroCrossing::PrintVectorData(int Type,int PulseNumber){

   const int SIZE = 200;
   char *outpath = new char[SIZE]; 
   if(Type==1) sprintf(outpath,"%d_midpoint_diagnostic.dat"     ,PulseNumber); 
   if(Type==2) sprintf(outpath,"%d_linear_interp_diagnostic.dat",PulseNumber); 
   if(Type==3) sprintf(outpath,"%d_least_squares_diagnostic.dat",PulseNumber); 

   const char *header = "# Zc \t Nc \t t0 (s) \t elapsed time (s) \t F (Hz)";

   fFileManager->DeleteFile(outpath); 

   double elapsed_time = 0; 
   const int N = fNZC; // fNCrossing.size();
   for(int i=0;i<N;i++){
      if(i==0){
         elapsed_time = 0;
      }else{
         elapsed_time = fTcross[i] - fTcross[i-1];
      }
      fFileManager->AppendToFile(outpath,header,fNCrossing[i],fNumCycles[i],fTcross[i],elapsed_time,fFreqAtCrossing[i]); 
   }

   delete[] outpath; 

}
//______________________________________________________________________________
int UMZeroCrossing::Calculate(UMPulse *aPulse){

   InitAnaArrays(); 

   int PulseNumber = aPulse->GetPulseNumber(); 
   int rc_fr=0,rc=0; 
   int zc_mid=0,zc_lin=0,zc_lsq=0;
   double nc_mid=0,nc_lin=0,nc_lsq=0; 
   double freq_mid=0,freq_lin=0,freq_lsq=0;
   double freq_mid_ph=0,freq_lin_ph=0,freq_lsq_ph=0;  // fit num cycles vs t_zc to a line 

   if(fUseMidpoint){
      CountZeroCrossings(1,aPulse);
      rc_fr       = CalculateFrequencies(zc_mid,freq_mid);
      freq_mid_ph = GetFrequencyFromPhaseFit(); 
      nc_mid      = ( (double)zc_mid - 1.)/2.; 
      PrintVectorData(1,PulseNumber); 
      Reset(); 
   }
   rc += rc_fr; 
   if(fUseLinearInterp){
      CountZeroCrossings(2,aPulse);
      rc_fr       = CalculateFrequencies(zc_lin,freq_lin);
      freq_lin_ph = GetFrequencyFromPhaseFit(); 
      nc_lin      = ( (double)zc_lin - 1.)/2.; 
      PrintVectorData(2,PulseNumber); 
      Reset(); 
   }
   rc += rc_fr; 
   if(fUseLeastSq){
      CountZeroCrossings(3,aPulse);
      rc_fr       = CalculateFrequencies(zc_lsq,freq_lsq);
      freq_lsq_ph = GetFrequencyFromPhaseFit(); 
      nc_lsq      = ( (double)zc_lsq - 1.)/2.; 
      PrintVectorData(3,PulseNumber); 
      Reset(); 
   }
   rc += rc_fr; 

   // store results

   // check if not a number 
   if( !UMMath::IsNaN(freq_mid) )    fFREQ[0] = freq_mid; 
   if( !UMMath::IsNaN(freq_lin) )    fFREQ[1] = freq_lin; 
   if( !UMMath::IsNaN(freq_lsq) )    fFREQ[2] = freq_lsq; 

   if( !UMMath::IsNaN(freq_mid_ph) ) fFREQ_ph[0] = freq_mid_ph; 
   if( !UMMath::IsNaN(freq_lin_ph) ) fFREQ_ph[1] = freq_lin_ph;
   if( !UMMath::IsNaN(freq_lsq_ph) ) fFREQ_ph[2] = freq_lsq_ph;

   fZC[0]   = zc_mid;    
   fZC[1]   = zc_lin;    
   fZC[2]   = zc_lsq;    

   fNC[0]   = nc_mid;    
   fNC[1]   = nc_lin;    
   fNC[2]   = nc_lsq;    

   Reset(); 

   return rc; 

}
//______________________________________________________________________________
void UMZeroCrossing::CountZeroCrossings(int method,UMPulse *aPulse){
   fNZC = UMMath::CountZeroCrossings(fVerbosity,method,fNPTS,fStep,fUseTimeRange,fTMin,fTMax,
                                      aPulse,fX,fY,fEY,fNCrossing,fCrossingIndex,fTcross,fVcross);
}
//______________________________________________________________________________
int UMZeroCrossing::CalculateFrequencies(int &TrueNumCrossings,double &FreqFullRange){

   if(fVerbosity>=3) std::cout << "[UMZeroCrossing]: Calculating frequencies..." << std::endl;

   int ret_code = 0; 
  
   int SIZE = fNZC; // fNCrossing.size();  
   int NumCrossings = SIZE;

   if(NumCrossings==0) std::cout << "[UMZeroCrossing::CalculateFrequencies]: NumCrossings is zero!" << std::endl;

   if(fUseIntegerCycles){
      /// if Zc is even, then number of cycles is odd; we want even cycles so we step back 1 bin
      if(NumCrossings%2==0){
         if(fVerbosity>=3) std::cout << "[UMZeroCrossing::CalculateFrequencies]: NumCrossings = " << NumCrossings << std::endl;
         NumCrossings -= 1;
         if(fVerbosity>=3) std::cout << "[UMZeroCrossing::CalculateFrequencies]: Adjusted to NumCrossings = " << NumCrossings << std::endl;
      }
   }

   TrueNumCrossings = NumCrossings;

   double Zc = (double)NumCrossings;  
   double NC = (Zc - 1.)/2.;  

   if(NumCrossings<5){
      std::cout << "[UMZeroCrossings::CalculateFrequencies]: ERROR!  Number of zero crossings is less than 5! " << std::endl;
      FreqFullRange = 0; 
      return 1;  
   }

   // find frequency using all zero crossings 
   // find the index of last crossing  
   int LastCrossingIndex  = NumCrossings - 1;  // the -1 is to account for arrays starting at 0 and not 1
    
   // compute the time 
   double delta_t  = fTcross[LastCrossingIndex] - fTcross[0];      
   // compute the frequency  
   FreqFullRange = NC/delta_t;   

   if(fVerbosity>=4){
      printf("[UMZeroCrossing::CalculateFrequencies]: time at first crossing: %.7f us \n",1E+6*fTcross[0]                );
      printf("[UMZeroCrossing::CalculateFrequencies]: time at last crossing:  %.7f us \n",1E+6*fTcross[LastCrossingIndex]);
   }

   double iNc=0,ifreq=0; 

   // now calculate the frequency using each crossing  
   for(int i=0;i<NumCrossings;i++){
      iNc        = ( (double)fNCrossing[i] - 1. )/2.;  
      if(i==0){
	 delta_t = 0; 
	 ifreq   = 0;
      }else{
	 delta_t = fTcross[i] - fTcross[0]; 
	 ifreq   = iNc/delta_t; 
      }
      fNumCycles[i]      = iNc;
      fFreqAtCrossing[i] = ifreq; 
   }

   if(fVerbosity>=4) printf("[UMZeroCrossing::CalculateFrequencies]: NumCycles =   %.3f \t delta_t = %.7f us \t f = %.7f Hz \n",NC,1E+6*delta_t,FreqFullRange); 
   
   return ret_code; 

}
//______________________________________________________________________________
double UMZeroCrossing::GetFrequencyFromPhaseFit(){
   double freq=0,intercept=0,r=0;
   UMMath::LeastSquaresFitting(fNZC,fTcross,fNumCycles,intercept,freq,r); 
   return freq;
}


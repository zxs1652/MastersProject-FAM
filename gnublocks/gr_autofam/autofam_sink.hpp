/*
	gr-autofam - A GNU Radio spectral auto-correlation density function
	Youssef Bagoulla

	Created this code by starting with a skeleton of gr_scan which I'm
	using to help with gnu-radio input syntax.  The algorithms have been
	taken from the matlab script AUTOFAM.
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/

#include <ctime>
#include <set>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iostream>

#include <boost/shared_ptr.hpp>

#include <string>
#include <gr_block.h>
#include <gr_io_signature.h>
#include <osmosdr_source_c.h>

class autofam_sink : public gr_block
{
	// source, vector_length, centerFreq, sampleRate, freqRes, cycFreqRes, fileName)
	public:
		autofam_sink(osmosdr_source_c_sptr source, unsigned int vector_length, double centerFreq, double sampleRate, double freqRes, double cycFreqRes, std::string fileName) :
			gr_block ("autofam_sink",
				gr_make_io_signature (1, 1, sizeof (float) * vector_length),
				gr_make_io_signature (0, 0, 0)),
			m_source(source), //We need the source in order to be able to control it
			m_buffer(new float[vector_length]), //buffer into which we accumulate the total for averaging
			m_vector_length(vector_length), //size of the FFT
		    m_fileName(fileName)
		{
			ZeroBuffer();
			filestr.open (m_fileName.c_str(), std::fstream::in | std::fstream::out | std::fstream::app);

		}

		virtual ~autofam_sink()
		{
			delete []m_buffer; //delete the buffer
		}
		
	private:
		virtual int general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
		{

//			for (int i = 0; i < ninput_items[0]; i++){
//				ProcessVector(((float *)input_items[0]) + i * m_vector_length);
//			}
//
//			consume_each(ninput_items[0]);
			return 0;
		}
		
		void ProcessVector(float *input)
		{
			/*
			//Add the FFT to the total
			for (unsigned int i = 0; i < m_vector_length; i++){
				m_buffer[i] += input[i];
			}
			m_count++; //increment the total

			if (m_avg_size == m_count){ //we've averaged over the number we intended to
				double freqs[m_vector_length]; //for convenience
				float bands0[m_vector_length]; //bands in order of frequency
				float bands1[m_vector_length]; //fine window bands
				float bands2[m_vector_length]; //coarse window bands

				// So bands0 is just the fftshift of the data, and freqs becomes the cooresponding frequencies.  So you could do plot(freqs, bands0)
				Rearrange(bands0, freqs, m_centre_freq_1, m_bandwidth0); //organize the buffer into a convenient order (saves to bands0)
				GetBands(bands0, bands1, m_bandwidth1); //apply the fine window (saves to bands1)
				GetBands(bands0, bands2, m_bandwidth2); //apply the coarse window (saves to bands2)

				PrintSignals(freqs, bands1, bands2);

				m_count = 0; //next time, we're starting from scratch - so note this
				ZeroBuffer(); //get ready to start again

				m_wait_count++; //we've just done another listen
				if (m_time/(m_bandwidth0/(double)(m_vector_length * m_avg_size)) <= m_wait_count){ //if we should move to the next frequency
					while (true) { //keep moving to the next frequency until we get to one we can listen on (copes with holes in the tunable range)
						if (m_centre_freq_2 <= m_centre_freq_1){ //we reached the end!
							//do something to end the scan
							fprintf(stderr, "[*] Finished scanning\n"); //say we're exiting
							exit(0); //TODO: This probably isn't the right thing, but it'll do for now
						}

						m_centre_freq_1 += m_step; //calculate the frequency we should change to
						double actual = m_source->set_center_freq(m_centre_freq_1); //change frequency
						if ((m_centre_freq_1 - actual < 10.0) && (actual - m_centre_freq_1 < 10.0)){ //success
							break; //so stop changing frequency
						}
					}
					m_wait_count = 0; //new frequency - we've listenned 0 times on it
				}
			}
			*/
		}
		
		void PrintSignals(double *freqs, float *bands1, float *bands2)
		{
			/* Calculate the current time after start */
			unsigned int t = time(0) - m_start_time;
			unsigned int hours = t / 3600;
			unsigned int minutes = (t % 3600) / 60;
			unsigned int seconds = t % 60;


			//Print that we finished scanning something
			fprintf(stderr, "%02u:%02u:%02u: Finished scanning %f MHz - %f MHz\n",
				hours, minutes, seconds, (m_centre_freq_1 - m_bandwidth0/2.0)/1000000.0, (m_centre_freq_1 + m_bandwidth0/2.0)/1000000.0);

			/* Calculate the differences between the fine and coarse window bands */
			float diffs[m_vector_length];
			for (unsigned int i = 0; i < m_vector_length; i++){
				diffs[i] = bands1[i] - bands2[i];
			}

			/* Look through to find signals */
			//start with no signal found (note: diffs[0] should always be very negative because of the way the windowing function works)
			bool sig = false;
			unsigned int peak = 0;
			for (unsigned int i = 0; i < m_vector_length; i++){
				if (sig){ //we're already in a signal
					if (diffs[peak] < diffs[i]){ //we found a rough end to the signal
						peak = i;
					}

					if (diffs[i] < m_threshold){ //we're transitionning to the end
						/* look for the "start" of the signal */
						unsigned int min = peak; //scan outwards for the minimum
						while ((diffs[min] > diffs[peak] - 3.0) && (min > 0)){ //while the signal is still more than half power
							min--;
						}

						/* look for the "end" */
						unsigned int max = peak;
						while ((diffs[max] > diffs[peak] - 3.0) && (max < m_vector_length - 1)){
							max++;
						}
						sig = false; //we're now in no signal state

						/* Print the signal if it's a genuine hit */
						if (TrySignal(freqs[max], freqs[min])){
							filestr.precision(2);
							filestr << hours
									<<":" << minutes
									<< ":" << seconds
									<< ": Found signal: at "
									<< (freqs[max] + freqs[min])/2000000.0
									<< "MHz of width "
									<< (freqs[max] - freqs[min])/1000.0
									<< "kHz, peak power "
									<< bands1[peak]
									<< "dB (difference "
									<< diffs[peak]
									<< "dB)" << std::endl;
							fprintf(stderr, "[+] %02u:%02u:%02u: Found signal: at %f MHz of width %f kHz, peak power %f dB (difference %f dB)\n",
								hours, minutes, seconds, (freqs[max] + freqs[min]) / 2000000.0, (freqs[max] - freqs[min])/1000.0, bands1[peak], diffs[peak]);
						}
					}
				} else {
					if (diffs[i] >= m_threshold){ //we found a signal!
						peak = i;
						sig = true;
					}
				}
			}
		}
		
		/**
		 * I should double check to make sure this works right, looks like he's transposing min and max on the calls to it.
		 */
		bool TrySignal(double min, double max)
		{
			double mid = (min + max)/2.0; //calculate the midpoint of the signal

			/* check to see if the signal is too close to the centre frequency (a signal often erroniously appears there) */
			if ((mid - m_centre_freq_1 < m_spread) && (m_centre_freq_1 - mid < m_spread)){
				return false; //if so, this is not a genuine hit
			}

			/* check to see if the signal is close to any other (the same signal often appears with a slightly different centre frequency) */
			std::set<double>::iterator it;
			double signal;
			for (it = m_signals.begin(); it != m_signals.end(); it++) {
				signal = *it;
				if ((mid - signal < m_spread) && (signal - mid < m_spread)){ //tpo close
					return false; //if so, this is not a genuine hit
				}
			}

			/* Genuine hit!:D */
			m_signals.insert(mid); //add to the set of signals
			return true; //genuine hit
		}
		
		void Rearrange(float *bands, double *freqs, double centre, double bandwidth)
		{
			double samplewidth = bandwidth/(double)m_vector_length;
			for (unsigned int i = 0; i < m_vector_length; i++){
				/* FFT is arranged starting at 0 Hz at the start, rather than in the middle */
				if (i < m_vector_length/2){ //lower half of the fft
					bands[i + m_vector_length/2] = m_buffer[i]/(float)m_avg_size;
				}
				else { //upper half of the fft
					bands[i - m_vector_length/2] = m_buffer[i]/(float)m_avg_size;
				}

				freqs[i] = centre + i * samplewidth - bandwidth/2; //calculate the frequency of this sample
			}
		}
		
		/**
		 * This function is going to take in a bandwidth and will do a moving average across the entire FFT window
		 * As the window slowly moves across the FFT's length, it averages in the current value of the FFT.
		 */
		void GetBands(float *powers, float *bands, unsigned int bandwidth)
		{
			double samplewidth = m_bandwidth0/(double)m_vector_length; //the width in Hz of each sample
			unsigned int bandwidth_samples = bandwidth/samplewidth; //the number of samples in our window

			for (unsigned int i = 0; i < m_vector_length; i++){ //we're averaging, so start with 0
				bands[i] = 0.0;
			}

			for (unsigned int i = 0; i < m_vector_length; i++){ //over the entire FFT
				 //make the buffer contains the entire window
				if ((i >= bandwidth_samples/2) && (i < m_vector_length + bandwidth_samples/2 - bandwidth_samples)){
					for (unsigned int j = 0; j < bandwidth_samples; j++){ //iterate over the window for averaging
						bands[i + j - bandwidth_samples/2] += powers[i] / (float)bandwidth_samples; //add this sample to the bands
					}
				}
			}
		}
		
		void ZeroBuffer()
		{
			/* writes zeros to m_buffer */
			for (unsigned int i = 0; i < m_vector_length; i++){
				m_buffer[i] = 0.0;
			}
		}
		
		//std::set<std::pair<double, double>> m_signals;
		std::set<double> m_signals;
		osmosdr_source_c_sptr m_source;
		float *m_buffer;
		unsigned int m_vector_length;
		unsigned int m_count;
		unsigned int m_wait_count;
		unsigned int m_avg_size;
		double m_step;
		double m_centre_freq_1;
		double m_centre_freq_2;
		double m_bandwidth0;
		double m_bandwidth1;
		double m_bandwidth2;
		double m_threshold;
		double m_spread;
		double m_time;
		time_t m_start_time;
		std::string m_fileName;
		std::fstream filestr;
};

/* Shared pointer thing gnuradio is fond of */
typedef boost::shared_ptr<autofam_sink> autofam_sink_sptr;
autofam_sink_sptr make_autofam_sink(osmosdr_source_c_sptr source, unsigned int vector_length, double centerFreq, double sampleRate, double freqRes, double cycFreqRes, std::string fileName)
{
	return boost::shared_ptr<autofam_sink>(new autofam_sink(source, vector_length, centerFreq, sampleRate, freqRes, cycFreqRes, fileName));
}

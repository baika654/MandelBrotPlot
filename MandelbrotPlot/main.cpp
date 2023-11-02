#define _USE_MATH_DEFINES
#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <cstdint>
#include "Mandelbrot.h"
#include <memory>
#include <math.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>




LRESULT CALLBACK WindowProcessMessages(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam);
void draw(HDC hdc, HWND hwnd);

class ThreadPool
   {
   public:
     ThreadPool (int threads): shutdown_ (false)
        {
        threads_.reserve (threads);
        for (int i =0; i < threads; ++i)
            threads_.emplace_back (std::bind (&ThreadPool::threadEntry, this, i));
        }
     
      ~ThreadPool ()
      {
        {
            // Unblock any threads and tell them to stop
            std::unique_lock <std::mutex> l (lock_);

            shutdown_ = true;
            condVar_.notify_all();
        }

        // Wait for all threads to stop
        std::cerr << "Joining threads" << std::endl;
        for (auto& thread : threads_)
            thread.join();
      }

      void doJob (std::function <void (void)> func)
         {
           // Place a job on the queu and unblock a thread
           std::unique_lock <std::mutex> l (lock_);
           njobs_pending++;
           jobs_.emplace (std::move (func));
           condVar_.notify_one();
         }

      void waitUntilCompleted() 
         {
         std::unique_lock<std::mutex> lock(main_mutex);
         main_condition.wait(lock);
         }
    
   protected:

      void threadEntry (int i)
      {
        std::function <void (void)> job;

        while (1)
        {
            {
                std::unique_lock <std::mutex> l (lock_);

                while (! shutdown_ && jobs_.empty())
                    condVar_.wait (l);

                if (jobs_.empty ())
                {
                    // No jobs to do and we are shutting down
                    main_condition.notify_one();
                    std::cerr << "Thread " << i << " terminates" << std::endl;
                    return;
                 }

                std::cerr << "Thread " << i << " does a job" << std::endl;
                job = std::move (jobs_.front ());
                jobs_.pop();
            }

            // Do the job without holding any locks
            job ();
        }

      }
    std::mutex lock_;
    std::condition_variable condVar_;
    bool shutdown_;
    std::queue <std::function <void (void)>> jobs_;
    std::vector <std::thread> threads_;
    std::mutex main_mutex;
  public:  
    
    std::atomic<int> njobs_pending;
    std::condition_variable main_condition;
   };


void calculate_part(int start_height, int end_height, std::shared_ptr<int[]> fractal, std::shared_ptr<int[]> histogram, ThreadPool *p);

int const WIDTH = 1200;
int const HEIGHT = 900;
bool fractalDrawn(false);



 


void spectral_color(double &r, double &g, double &b, double l) // RGB <0,1> <- lambda l <400,700> [nm]
   {
   double t;  r = 0.0; g = 0.0; b = 0.0;
   if ((l >= 400.0) && (l < 410.0)) { t = (l - 400.0) / (410.0 - 400.0); r = +(0.33*t) - (0.20*t*t); }
   else if ((l >= 410.0) && (l < 475.0)) { t = (l - 410.0) / (475.0 - 410.0); r = 0.14 - (0.13*t*t); }
   else if ((l >= 545.0) && (l < 595.0)) { t = (l - 545.0) / (595.0 - 545.0); r = +(1.98*t) - (t*t); }
   else if ((l >= 595.0) && (l < 650.0)) { t = (l - 595.0) / (650.0 - 595.0); r = 0.98 + (0.06*t) - (0.40*t*t); }
   else if ((l >= 650.0) && (l < 700.0)) { t = (l - 650.0) / (700.0 - 650.0); r = 0.65 - (0.84*t) + (0.20*t*t); }
   if ((l >= 415.0) && (l < 475.0)) { t = (l - 415.0) / (475.0 - 415.0); g = +(0.80*t*t); }
   else if ((l >= 475.0) && (l < 590.0)) { t = (l - 475.0) / (590.0 - 475.0); g = 0.8 + (0.76*t) - (0.80*t*t); }
   else if ((l >= 585.0) && (l < 639.0)) { t = (l - 585.0) / (639.0 - 585.0); g = 0.84 - (0.84*t); }
   if ((l >= 400.0) && (l < 475.0)) { t = (l - 400.0) / (475.0 - 400.0); b = +(2.20*t) - (1.50*t*t); }
   else if ((l >= 475.0) && (l < 560.0)) { t = (l - 475.0) / (560.0 - 475.0); b = 0.7 - (t)+(0.30*t*t); }
   }


int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, PSTR cmdLine, INT cmdCount) {
   
   
   // Initialize GDI+
   Gdiplus::GdiplusStartupInput gdiplusStartupInput;
   ULONG_PTR gdiplusToken;
   Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
   
   const char*CLASS_NAME = "myWin32WindowClass";
   WNDCLASS wc{};
   wc.hInstance = currentInstance;
   wc.lpszClassName = CLASS_NAME;
   wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
   wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
   wc.lpfnWndProc = WindowProcessMessages;
   RegisterClass(&wc);

   // Create the window
   CreateWindow(CLASS_NAME, "Win32 Tutorial", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT , CW_USEDEFAULT, WIDTH, HEIGHT, nullptr, nullptr, nullptr, nullptr);

   // Window loop

   
   MSG msg{};
   while (GetMessage
   (&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      }

   
   Gdiplus::GdiplusShutdown(gdiplusToken);
   return 0;
   }

LRESULT CALLBACK WindowProcessMessages(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam) {
   HDC hdc;
   PAINTSTRUCT ps;

   switch (msg) {
      case WM_PAINT:
         hdc = BeginPaint(hwnd, &ps);
         draw(hdc, hwnd);
         EndPaint(hwnd, &ps);
         return 0;
      case WM_DESTROY:
         PostQuitMessage(0);
         return 0;
      default:
         return DefWindowProc(hwnd, msg, param, lparam);
      }
   }

//std::wstring GetWC(const char *c)
//{
//    const size_t cSize = strlen(c)+1;
//    std::wstring wc( cSize, L'#' );
//    mbstowcs( &wc[0], c, cSize );
//    return wc;
//}

void draw(HDC hdc, HWND hwnd) {
   
   using namespace std::chrono;
   std::cout << "Starting the caclulations" << std::endl;
   auto start = high_resolution_clock::now();

   double min = 999999;
   double max = -999999;

   std::shared_ptr<int[]> histogram(new int[Mandelbrot::MAX_ITERATIONS]{ 0 });
   std::shared_ptr<int[]> fractal(new int[WIDTH*HEIGHT]);

   // First part
   
   /*
   for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
         double xFractal = (x - WIDTH / 2 - 200) * 2.0 / HEIGHT;
         double yFractal = (y - HEIGHT / 2) * 2.0 / HEIGHT;

         int iterations = Mandelbrot::getIterations(xFractal, yFractal);

         fractal[y*WIDTH + x] = iterations;

         if (iterations != Mandelbrot::MAX_ITERATIONS) {
            histogram[iterations]++;
            }


         }
      }
   
   */
   // Second part

   
   int number_of_packets_of_work = 90;

   

   
   ThreadPool p (16);

   for (int i=0; i<number_of_packets_of_work; i++) 
      {
      p.doJob(std::bind (calculate_part,i * HEIGHT/number_of_packets_of_work ,(i+1) * HEIGHT/number_of_packets_of_work  ,fractal, histogram, &p));
      }

     
   
   p.waitUntilCompleted();
   
   
   

     

      int total = 0;
      for (int i = 0; i < Mandelbrot::MAX_ITERATIONS; i++) {
         total += histogram[i];
         }

      const int size = HEIGHT * WIDTH * 3;
      uint8_t *data;
      data = new uint8_t[size];
      int j = 0;


      for (int y = 0; y < HEIGHT; y++) {
         for (int x = 0; x < WIDTH; x++) {

            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            double red_d = 0.0;
            double green_d = 0.0;
            double blue_d = 0.0;
            double realLight = 0.0;

            int iterations = fractal[y*WIDTH + x];

            if (iterations != Mandelbrot::MAX_ITERATIONS) {
               double hue = 0.0;
               for (int i = 0; i <= iterations; i++) {
                  hue += ((double)histogram[i]) / total;
                  
                  }


                realLight = pow(300.0, hue) + 400;
               }
            

            spectral_color(red_d, green_d, blue_d, realLight);

            data[j] = (uint8_t)(255 *red_d/(red_d+green_d+blue_d));
            data[j + 1] = (uint8_t)(255 * green_d / (red_d + green_d + blue_d));
            data[j + 2] = (uint8_t)(255 * blue_d / (red_d + green_d + blue_d));
            j += 3;
            }


         }
      BITMAPINFOHEADER bmih;
      bmih.biBitCount = 24;
      bmih.biClrImportant = 0;
      bmih.biClrUsed = 0;
      bmih.biCompression = BI_RGB;
      bmih.biWidth = WIDTH;
      bmih.biHeight = HEIGHT;
      bmih.biPlanes = 1;
      bmih.biSize = 40;
      bmih.biSizeImage = size;

      BITMAPINFO bmpi;
      bmpi.bmiHeader = bmih;
      SetDIBitsToDevice(hdc, 0, 0, WIDTH, HEIGHT, 0, 0, 0, HEIGHT, data, &bmpi, DIB_RGB_COLORS);
      delete[] data;
      auto stop = high_resolution_clock::now();
      auto duration = duration_cast<seconds>(stop-start);
      char buffer[256];
      sprintf_s(buffer, "Calculations took %d seconds.", duration.count());
      RECT rect;
      GetClientRect (hwnd, &rect) ;
      SetTextColor(hdc, 0xFFFFFFFF);
      SetBkMode(hdc,TRANSPARENT);
      rect.left=40;
      rect.top=10;

      DrawText( hdc, buffer , -1, &rect, DT_SINGLELINE | DT_NOCLIP  ) ;

   }


   void calculate_part(int start_height, int end_height, std::shared_ptr<int[]> fractal, std::shared_ptr<int[]> histogram, ThreadPool *p)
      {
      for (int y = start_height; y < end_height; y++) {
            for (int x = 0; x < WIDTH; x++) {
               double xFractal = (x - WIDTH / 2 - 200) * 2.0 / HEIGHT;
               double yFractal = (y - HEIGHT / 2) * 2.0 / HEIGHT;

               int iterations = Mandelbrot::getIterations(xFractal, yFractal);

               fractal[y*WIDTH + x] = iterations;

               if (iterations != Mandelbrot::MAX_ITERATIONS) {
                  histogram[iterations]++;
                  }


               }
            }
      if (--(p->njobs_pending) ==0)
         {
            p->main_condition.notify_one();
         }

      }
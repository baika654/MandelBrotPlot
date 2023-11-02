#pragma once
class Mandelbrot
   {
   public:
      static const int MAX_ITERATIONS = 5000;


   public:
      Mandelbrot();
      virtual ~Mandelbrot();

      static int getIterations(double x, double y);

   };


#include "livewire.h"

#include <QMessageBox>

LiveWire::LiveWire()
{
  m_image = 0;
  m_grad = 0;
  m_tmp = 0;
  m_edgeWeight.clear();
  m_cost.clear();
  m_prev.clear();

  m_gradImage = QImage(100, 100, QImage::Format_Indexed8);

  m_width = m_height = 0;

  m_poly.clear();
  m_livewire.clear();
}

LiveWire::~LiveWire() { reset(); }

void
LiveWire::resetPoly()
{
  m_poly.clear();
  m_livewire.clear();
}

void
LiveWire::reset()
{
  if (m_image) delete [] m_image;
  if (m_grad) delete [] m_grad;
  if (m_tmp) delete [] m_tmp;

  m_image = 0;
  m_grad = 0;
  m_tmp = 0;
  m_edgeWeight.clear();
  m_cost.clear();
  m_prev.clear();

  m_width = m_height = 0;

  m_poly.clear();
  m_livewire.clear();
}

QVector<QPoint> LiveWire::poly() { return m_poly; }
QVector<QPoint> LiveWire::livewire() { return m_livewire; }

void
LiveWire::setImageData(int w, int h, uchar *img)
{
  if (m_width != w || m_height != h)
    {
      if (m_image) delete [] m_image;
      if (m_grad) delete [] m_grad;
      if (m_tmp) delete [] m_tmp;

      m_width = w;
      m_height = h;

      m_image = new uchar[m_width*m_height];
      m_grad = new float[sizeof(float)*m_width*m_height];  
      m_tmp = new uchar[4*m_width*m_height];  

      m_edgeWeight.clear();
      m_cost.clear();
      m_prev.clear();
      m_edgeWeight.resize(8*m_width*m_height);
      m_cost.resize(m_width*m_height);
      m_prev.resize(m_width*m_height);
    }

  memcpy(m_image, img, m_width*m_height);
  
  applySmoothing(1);
  calculateGradients();
  calculateEdgeWeights();
}

bool
LiveWire::mousePressEvent(QMouseEvent *event)
{
  QPoint pp = event->pos();
  float ypos = pp.y();
  float xpos = pp.x();
  return mousePressEvent(xpos, ypos, event);
}

bool
LiveWire::mouseMoveEvent(QMouseEvent *event)
{
  QPoint pp = event->pos();
  float ypos = pp.y();
  float xpos = pp.x();

  return mouseMoveEvent(xpos, ypos, event);
}

bool
LiveWire::mousePressEvent(int xpos, int ypos, QMouseEvent *event)
{
  int button = event->button();
  
//  bool shiftModifier = event->modifiers() & Qt::ShiftModifier;
//  bool ctrlModifier = event->modifiers() & Qt::ControlModifier;
//  bool altModifier = event->modifiers() & Qt::AltModifier;

  if (button == Qt::LeftButton)
    {
      m_poly += m_livewire;
      m_poly << QPoint(xpos, ypos);

      m_livewire.clear();
      calculateCost(xpos, ypos, 500);

      return true;
    }

  if (button == Qt::RightButton)
    {
      for(int i=m_poly.count()-1; i>=0; i--)
	{
	  if ((m_poly[i]-QPoint(xpos, ypos)).manhattanLength() < 3)
	    {
	      m_poly.remove(i, m_poly.count()-i);

	      m_livewire.clear();
	      calculateCost(xpos, ypos, 500);
	      break;
	    }
	}
      return true;
    }

  return false;
}

bool
LiveWire::mouseMoveEvent(int xpos, int ypos, QMouseEvent *event)
{
//  int button = event->button();
//  
//  bool shiftModifier = event->modifiers() & Qt::ShiftModifier;
//  bool ctrlModifier = event->modifiers() & Qt::ControlModifier;
//  bool altModifier = event->modifiers() & Qt::AltModifier;

  if (m_poly.count() > 0)
    {
      calculateLivewire(xpos, ypos);
      return true;
    }
  
  return false;
}

void LiveWire::mouseReleaseEvent(QMouseEvent *event) {}

bool
LiveWire::keyPressEvent(QKeyEvent *event)
{
  bool shiftModifier = event->modifiers() & Qt::ShiftModifier;
  bool ctrlModifier = event->modifiers() & Qt::ControlModifier;
  bool altModifier = event->modifiers() & Qt::AltModifier;

  if (event->key() == Qt::Key_Escape)
    {
      resetPoly();
      return true;
    }
    
  return false;
}

QImage
LiveWire::gradientImage()
{
  return m_gradImage;
}

void
LiveWire::applySmoothing(int sz)
{
  memcpy(m_tmp, m_image, m_width*m_height);
  gaussBlur_4(m_tmp, m_image, m_width, m_height, sz);
}

void
LiveWire::calculateGradients()
{
//  int sobel_x[3][3] = {{-1,  0,  1},
//			 {-2,  0,  2},
//			 {-1,  0,  1}, };
//
//  int sobel_y[3][3] = {{-1, -2, -1},
//			 { 0,  0,  0},
//			 { 1,  2,  1}, };  

  memset(m_grad, 0, sizeof(float)*m_width*m_height);

  // sobel
  float maxGrad = 0;
  float minGrad = 1000000;
  for(int i=1; i<m_height-1; i++)
    for(int j=1; j<m_width-1; j++)
      {	
	float dx = 0;
	float dy = 0;
	{
	  dx += (m_image[(i+1)*m_width+(j-1)] -
		 m_image[(i-1)*m_width+(j-1)]);
	  dx += 2*(m_image[(i+1)*m_width+j] -
		   m_image[(i-1)*m_width+j]);
	  dx += (m_image[(i+1)*m_width+(j+1)] -
		 m_image[(i-1)*m_width+(j+1)]);
	  
	  dy += (m_image[(i+1)*m_width+(j+1)] -
		 m_image[(i+1)*m_width+(j-1)]);
	  dy += 2*(m_image[i*m_width+(j+1)] -
		   m_image[i*m_width+(j-1)]);
	  dy += (m_image[(i-1)*m_width+(j+1)] -
		 m_image[(i-1)*m_width+(j-1)]);
	}

	float grd = qSqrt(dx*dx + dy*dy);
	maxGrad = qMax(maxGrad, grd);
	minGrad = qMin(minGrad, grd);
	m_grad[i*m_width+j] = grd;
      }

//  // central difference
//  float maxGrad = 0;
//  float minGrad = 1000000;
//  for(int i=1; i<m_height-1; i++)
//    for(int j=1; j<m_width-1; j++)
//      {	
//	float dx = 0;
//	float dy = 0;
//	{
//	  dx += (m_image[(i+1)*m_width+j] -
//		 m_image[(i-1)*m_width+j]);
//	  
//	  dy += (m_image[i*m_width+(j+1)] -
//		 m_image[i*m_width+(j-1)]);
//	}
//
//	float grd = qSqrt(dx*dx + dy*dy);
//	maxGrad = qMax(maxGrad, grd);
//	minGrad = qMin(minGrad, grd);
//	m_grad[i*m_width+j] = grd;
//      }

  for(int i=0; i<m_width*m_height; i++)
    m_grad[i] = 1.0-(m_grad[i]-minGrad)/(maxGrad-minGrad);


  for(int i=0; i<m_width*m_height; i++)
    {
      int c = 200*(1-m_grad[i]);
      m_tmp[4*i+0] = 0;
      m_tmp[4*i+1] = c/2;
      m_tmp[4*i+2] = c;
      m_tmp[4*i+3] = c;
    }

  m_gradImage = QImage(m_tmp,
		       m_width,
		       m_height,
		       QImage::Format_ARGB32);
}   

void
LiveWire::calculateEdgeWeights()
{  
  float sqrt2 = qSqrt(2.0f);

  for(int i=0; i<m_edgeWeight.count(); i++)
    m_edgeWeight[i] = 10000000;

  for(int i=0; i<m_height; i++)
    for(int j=0; j<m_width; j++)
      {
	int midx = i*m_width+j;
	for(int a=-1; a<=1; a++)
	  for(int b=-1; b<=1; b++)
	    {	      
	      int ia = i+a;
	      int jb = j+b;
	      if (ia < 0 || ia >= m_height ||
		  jb < 0 || jb >= m_width)
		{ }
	      else
		{
		  int idx = (a+1)*3+(b+1);
		  // 0  1  2
		  // 3  4  5
		  // 6  7  8
		  float scl = 1;
		  if (idx%2 == 0) scl = 1.414; // scale for diagonal links

		  if (idx != 4)
		    {
		      if (idx > 4) idx--;
		      // 0  1  2         0  1  2
		      // 3  4  5   ==>   3     4
		      // 6  7  8         5  6  7
		      m_edgeWeight[8*midx + idx] = scl;
		    }
		}
	    }
      }
}

void
LiveWire::calculateCost(int wpos, int hpos, int boxSize)
{
  if (wpos < 0 || wpos >= m_width ||
      hpos < 0 || hpos >= m_height)
    return;

  for(int i=0; i<m_cost.count(); i++)
    m_cost[i] = 10000000.0;
  
  for(int i=0; i<m_prev.count(); i++)
    m_prev[i] = QPoint(-1, -1);

  // used as priority queue
  QMultiMap<float, QPoint> qmap;

  bool *visited = new bool[m_width*m_height];
  memset(visited, 0, m_width*m_height);

  int x = hpos;
  int y = wpos;
  m_cost[x*m_width + y] = 0;
  qmap.insert(0, QPoint(x, y));

  while(qmap.count() > 0)
    {
      float key = qmap.firstKey();
      QList<QPoint> qpr = qmap.values(key);
      float dcost = key;
      int x = qpr[0].x();
      int y = qpr[0].y();
      qmap.remove(key, qpr[0]);
      if (qpr.count() == 1)
	qmap.remove(key);

      int midx = x*m_width+y;

      visited[midx] = true;
      
      // visit all neighbours
      for(int a=-1; a<=1; a++)
	for(int b=-1; b<=1; b++)
	  {
//	    if (x+a < 0 || x+a >= m_height ||
//		y+b < 0 || y+b >= m_width)
	    if (x+a < hpos-boxSize || x+a >= hpos+boxSize ||
		y+b < wpos-boxSize || y+b >= wpos+boxSize ||
		x+a < 0 || x+a >= m_height ||
		y+b < 0 || y+b >= m_width)
	      {}
	    else
	      {
		int idx = (a+1)*3+(b+1);
		// 0  1  2
		// 3  4  5
		// 6  7  8
		if (idx != 4 && !visited[(x+a)*m_width+(y+b)])
		  //if (idx != 4)
		  {
		    if (idx > 4) idx--;
		    // 0  1  2         0  1  2
		    // 3  4  5   ==>   3     4
		    // 6  7  8	       5  6  7
		    float wt = m_edgeWeight.at(8*midx + idx);
		    float newcost = dcost + wt*m_grad[midx];
		    //float newcost = dcost + wt*m_image[midx];
		    float oldcost = m_cost.at((x+a)*m_width+(y+b)); 
		    if (newcost < oldcost)
		      {
			m_cost[(x+a)*m_width+(y+b)] = newcost;
			m_prev[(x+a)*m_width+(y+b)] = QPoint(x, y);
			
			qmap.insert(newcost, QPoint(x+a, y+b));
		      }
		  }
	      }
	  }
      
    }

  delete [] visited;
}


void
LiveWire::calculateLivewire(int wpos, int hpos)
{
  if (wpos < 0 || wpos >= m_width ||
      hpos < 0 || hpos >= m_height)
    return;

  int x = hpos;
  int y = wpos;
  QVector<QPoint> pts;
  while(x > -1)
    {
      pts << QPoint(y, x);
      int idx = x*m_width+y;
      x = m_prev[idx].x();
      y = m_prev[idx].y();
    }

  m_livewire.clear();
  int pc = pts.count();
  for(int i=0; i<pc; i++)
    m_livewire << pts[pc-1-i];
}

// taken from http://blog.ivank.net/fastest-gaussian-blur.html
QVector<int>
LiveWire::boxesForGauss(float sigma, int n)  // standard deviation, number of boxes
{
  float wIdeal = qSqrt((12*sigma*sigma/n)+1);  // Ideal averaging filter width 
  int wl = wIdeal;
  if(wl%2==0) wl--;
  int wu = wl+2;
				
  float mIdeal = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4);
  int m = mIdeal;
  
  QVector<int> sizes;
  for(int i=0; i<n; i++)
    sizes << (i<m ? wl : wu);

  return sizes;
}
void
LiveWire::gaussBlur_4(uchar *scl, uchar *tcl,
		      int w, int h, int r)
{
  QVector<int> bxs = boxesForGauss(r, 3);
  boxBlur_4 (scl, tcl, w, h, (bxs[0]-1)/2);
  boxBlur_4 (tcl, scl, w, h, (bxs[1]-1)/2);
  boxBlur_4 (scl, tcl, w, h, (bxs[2]-1)/2);
}
void
LiveWire::boxBlur_4(uchar *scl, uchar *tcl,
		    int w, int h, int r)

{
  for(int i=0; i<w*h; i++)
    tcl[i] = scl[i];
  boxBlurH_4(tcl, scl, w, h, r);
  boxBlurT_4(scl, tcl, w, h, r);
}
void
LiveWire::boxBlurH_4(uchar *scl, uchar *tcl,
		     int w, int h, int r)
{
  float iarr = 1.0f/(r+r+1);
  for(int i=0; i<h; i++)
    {
      int ti = i*w;
      int li = ti;
      int ri = ti+r;
      int fv = scl[ti];
      int lv = scl[ti+w-1];
      int val = (r+1)*fv;
      for(int j=0; j<r; j++)
	val += scl[ti+j];
      for(int j=0; j<=r; j++)
	{
	  val += scl[ri++] - fv ;
	  tcl[ti++] = val*iarr;
	}
      for(int j=r+1; j<w-r; j++)
	{
	  val += scl[ri++] - scl[li++];
	  tcl[ti++] = val*iarr;
	}
      for(int j=w-r; j<w; j++)
	{
	  val += lv - scl[li++];
	  tcl[ti++] = val*iarr;
	}
  }
}
void
LiveWire::boxBlurT_4(uchar *scl, uchar *tcl,
		     int w, int h, int r)
{
  float iarr = 1.0f/(r+r+1);
  for(int i=0; i<w; i++)
    {
      int ti = i;
      int li = ti;
      int ri = ti+r*w;
      int fv = scl[ti];
      int lv = scl[ti+w*(h-1)];
      int val = (r+1)*fv;
      for(int j=0; j<r; j++)
	val += scl[ti+j*w];
      for(int j=0; j<=r; j++)
	{
	  val += scl[ri] - fv;
	  tcl[ti] = val*iarr;
	  ri+=w; ti+=w;
	}
    for(int j=r+1; j<h-r; j++)
      {
	val += scl[ri] - scl[li];
	tcl[ti] = val*iarr;
	li+=w; ri+=w; ti+=w;
      }
    for(int j=h-r; j<h; j++)
      {
	val += lv - scl[li];
	tcl[ti] = val*iarr;
	li+=w; ti+=w;
      }
  }
}

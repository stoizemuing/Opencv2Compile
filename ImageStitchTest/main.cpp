#include <QtCore/QCoreApplication>
#include <QVector3D>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>

using namespace std;
using namespace cv;
using namespace xfeatures2d;

struct four_corners_t
{
	Point2f left_top;
	Point2f left_bottom;
	Point2f right_top;
	Point2f right_bottom;
};

cv::Point2f calPerspectivePos(double *srcPos, const Mat& H)
{
	double dstPos[3];
	Mat V2 = Mat(3, 1, CV_64FC1, srcPos);  //列向量;
	Mat V1 = Mat(3, 1, CV_64FC1, dstPos);  //列向量
	V1 = H * V2;

	cv::Point2f point = cv::Point2f(dstPos[0] / dstPos[2], dstPos[1] / dstPos[2]);
	return point;
}

void OptimizeSeam(Mat& img1, Mat& trans, Mat& dst, four_corners_t& corners)
{
	int start = MIN(corners.left_top.x, corners.left_bottom.x);//开始位置，即重叠区域的左边界  

	double processWidth = img1.cols - start;//重叠区域的宽度  
	int rows = dst.rows;
	int cols = img1.cols; //注意，是列数*通道数
	double alpha = 1;//img1中像素的权重  
	for (int i = 0; i < rows; i++)
	{
		uchar* p = img1.ptr<uchar>(i);  //获取第i行的首地址
		uchar* t = trans.ptr<uchar>(i);
		uchar* d = dst.ptr<uchar>(i);
		for (int j = start; j < cols; j++)
		{
			//如果遇到图像trans中无像素的黑点，则完全拷贝img1中的数据
			if (t[j * 3] == 0 && t[j * 3 + 1] == 0 && t[j * 3 + 2] == 0)
			{
				alpha = 1;
			}
			else
			{
				//img1中像素的权重，与当前处理点距重叠区域左边界的距离成正比，实验证明，这种方法确实好  
				alpha = (processWidth - (j - start)) / processWidth;
			}

			d[j * 3] = p[j * 3] * alpha + t[j * 3] * (1 - alpha);
			d[j * 3 + 1] = p[j * 3 + 1] * alpha + t[j * 3 + 1] * (1 - alpha);
			d[j * 3 + 2] = p[j * 3 + 2] * alpha + t[j * 3 + 2] * (1 - alpha);

		}
	}

}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	//提取特征点   
	Mat image01 = imread("D:\\Work\\Code\\ImageStitchTest\\Image\\1.jpg");
	Mat image02 = imread("D:\\Work\\Code\\ImageStitchTest\\Image\\2.jpg");    //左图

	//灰度图转换  
	Mat image1, image2;
	cvtColor(image01, image1, CV_RGB2GRAY);
	cvtColor(image02, image2, CV_RGB2GRAY);

	//提取特征点    
	Ptr<SURF> surfDetector = SURF::create(2000);  // 海塞矩阵阈值，在这里调整精度，值越大点越少，越精准 
	vector<KeyPoint> keyPoint1, keyPoint2;
	surfDetector->detect(image1, keyPoint1);
	surfDetector->detect(image2, keyPoint2);

	//特征点描述，为下边的特征点匹配做准备    
	Ptr<SURF> SurfDescriptor = SURF::create();
	Mat imageDesc1, imageDesc2;
	SurfDescriptor->compute(image1, keyPoint1, imageDesc1);
	SurfDescriptor->compute(image2, keyPoint2, imageDesc2);

	//获得匹配特征点，并提取最优配对     
	FlannBasedMatcher matcher;
	vector<vector<DMatch>> matchePoints;
	vector<DMatch> GoodMatchePoints;

	vector<Mat> train_desc(1, imageDesc1);
	matcher.add(train_desc);
	matcher.train();

	matcher.knnMatch(imageDesc2, matchePoints, 2);

	for (int i = 0; i < matchePoints.size(); i++)
	{
		if (matchePoints[i][0].distance < 0.4 * matchePoints[i][1].distance)
		{
			GoodMatchePoints.push_back(matchePoints[i][0]);
		}
	}
	Mat first_match;
	drawMatches(image02, keyPoint2, image01, keyPoint1, GoodMatchePoints, first_match);
	namedWindow("match", 0);
	imshow("match", first_match);
	imwrite("match.jpg", first_match);

	vector<Point2f> imagePoints1, imagePoints2;

	for (int i = 0; i < GoodMatchePoints.size(); i++)
	{
		imagePoints2.push_back(keyPoint2[GoodMatchePoints[i].queryIdx].pt);
		imagePoints1.push_back(keyPoint1[GoodMatchePoints[i].trainIdx].pt);
	}

	//获取图像1到图像2的投影映射矩阵 尺寸为3*3  
	Mat homo = findHomography(imagePoints1, imagePoints2, CV_RANSAC);

	four_corners_t corners;
	double leftTop[3] = { 0,0,1 };
	corners.left_top = calPerspectivePos(leftTop, homo);
	double leftBottom[3] = { 0,double(image01.rows),1 };
	corners.left_bottom = calPerspectivePos(leftBottom, homo);
	double rightTop[3] = { double(image01.cols),0,1 };
	corners.right_top = calPerspectivePos(rightTop, homo);
	double rightBottom[3] = { double(image01.cols),double(image01.rows),1 };
	corners.right_bottom = calPerspectivePos(rightBottom, homo);

	//图像配准  
	Mat imageTransform1;
	cv::warpPerspective(image01, imageTransform1, homo, Size(MAX(corners.right_top.x, corners.right_bottom.x), image02.rows));
	imwrite("trans1.jpg", imageTransform1);

	int dst_width = imageTransform1.cols;  //取最右点的长度为拼接图的长度
	int dst_height = image02.rows;

	Mat dst(dst_height, dst_width, CV_8UC3);
	dst.setTo(0);

	imageTransform1.copyTo(dst(Rect(0, 0, imageTransform1.cols, imageTransform1.rows)));
	image02.copyTo(dst(Rect(0, 0, image02.cols, image02.rows)));

	OptimizeSeam(image02, imageTransform1, dst, corners);
	imwrite("dst.jpg", dst);

	waitKey();

    return a.exec();
}


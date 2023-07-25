### OpenCV使用踩雷

在编译的时候需要点击批生成，同时选取all-build和install.最后头文件和库文件都会在install中。

### NoFree模块使用踩雷

Opencv3中，SURF模块改变了使用方法，不能实例化使用，应该采用下面的方式来调用

```c++
Ptr<SURF> surfDetector = SURF::create(2000);  // 海塞矩阵阈值，在这里调整精度，值越大点越少，越精准 
vector<KeyPoint> keyPoint1, keyPoint2;
surfDetector->detect(image1, keyPoint1);
surfDetector->detect(image2, keyPoint2);

Ptr<SURF> SurfDescriptor = SURF::create();
Mat imageDesc1, imageDesc2;
SurfDescriptor->compute(image1, keyPoint1, imageDesc1);
SurfDescriptor->compute(image2, keyPoint2, imageDesc2);
```


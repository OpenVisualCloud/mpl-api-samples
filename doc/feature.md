# Feature Guide
## Library supported features
These are IMPL enabled features in detail.
### 1.CSC
CSC is short for color space conversion. A source pixel format can be translated to the target pixel format with CSC filters.
IMPL library supports the following color formats conversion.

| source format   | target format   | feature status |
| :---            |     :---        | :----:         |
| yuv422ycbcr10be | v210            | yes            |
| yuv422ycbcr10be | y210            | yes            |
| yuv422ycbcr10be | nv12            | yes            |
| yuv422ycbcr10be | yuv422p10le     | yes            |
| yuv422ycbcr10be | yuv420p10le     | yes            |
| yuv422ycbcr10be | i420            | yes            |
| yuv422ycbcr10be | p010            | yes            |
| v210            | y210            | yes            |
| v210            | yuv422p10le     | yes            |
| v210            | yuv422ycbcr10be | yes            |
| v210            | yuv422ycbcr10le | yes            |
| y210            | v210            | yes            |
| y210            | yuv422p10le     | yes            |
| y210            | yuv422ycbcr10be | yes            |
| yuv422p10le     | v210            | yes            |
| yuv422p10le     | y210            | yes            |
| yuv422ycbcr10le | v210            | yes            |
| yuv422ycbcr10le | y210            | yes            |

### 2.Resize
Resizing alters the video's resolution. IMPL resize supports 6 formats. Interpolation method can be chosen as bilinear or bicubic. Bicubic method gets higher quality but slower than bilinear method. In IMPL resize process, the accuracy of interpolation computing result is float. Besides, IMPL resize supports to put the result somewhere in a larger size video, more details can be read in IMPL API.

| source format   | interpolation method  | -accuracy | feature status |
| :---            |     :---              | :----:    | :----:         |
| yuv422ycbcr10be | bilinear/bicubic      | float     | yes            |
| i420            | bilinear/bicubic      | float     | yes            |
| v210            | bilinear/bicubic      | float     | yes            |
| yuv420p10le     | bilinear/bicubic      | float     | yes            |
| p010            | bilinear/bicubic      | float     | yes            |
| y210            | bilinear/bicubic      | float     | yes            |

### 3.Composition
Composition combines several videos with a background video, arranging them within one screen. IMPL composition supports the overlay of up to twenty videos. IMPL composition also supports to crop the smaller video at first and then composites it to the larger video.

| source format   | feature status |
| :---            | :----:         |
| i420            | yes            |
| v210            | yes            |

### 4.Alpha blending
Alpha blending function has used graphics overlay to create special effects with multiple video streams which is required to generate a composite pixel. Alpha blending requires fast hardware since the video processing must be done at pixel rates. Comparing with Composition, each pixel of foreground videos needs an additional transparency data called alpha value. IMPL Alpha blending supports both static alpha and alpha surface per frame. Static alpha means transparency value of all pixels is the same for a frame. For alpha surface, each pixel has a different transparency value stored in a file, ranging from 0 to 255.

| source format   | feature status |
| :---            | :----:         |
| i420            | yes            |
| v210            | yes            |
| yuv420p10le     | yes            |

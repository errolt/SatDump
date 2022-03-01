#include "plugin.h"
#include "logger.h"
#include "settings.h"
#include "resources.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include "libs/predict/predict.h"
#include "modules/goes/gvar/module_gvar_image_decoder.h"
#include "tle.h"
#include "common/geodetic/projection/geo_projection.h"
#include "common/map/map_drawer.h"

#define FIXED_FLOAT(x) std::fixed << std::setprecision(3) << (x)
struct  crop_t
{
    float x1;           //Top left corner of image crop
    float y1;
    float x2;           //Lower right corner of image crop
    float y2;
    std::string name;   //File name for crop, unused for composite
    std::string channel;//Channel for source image
    bool include_map;   //Should map be overlayed
    bool include_date;  //Should file name include date/time
};

struct images_t
{
    crop_t crop;    //Source image info
    uint16_t x;     //Final section location on composite
    uint16_t y;
    uint16_t width; //Final section width & height
    uint16_t height;
};
struct composite_t
{
    std::string name;   //File name for composite
    bool include_date;  //Should file name include date/time
    uint16_t width;     //Final composite width & height
    uint16_t height;
    std::vector<images_t> images;
};

geodetic::projection::GEOSProjection proj_geos;

class GVARExtended : public satdump::Plugin
{
private:
    static std::string misc_preview_text;
    static std::vector<std::array<float, 3>> points;
    static std::vector<crop_t> crops;
    static std::vector<composite_t> composites;

    static std::vector<std::string> names;

    static void satdumpStartedHandler(const satdump::SatDumpStartedEvent &)
    {
        crops.clear();
        composites.clear();
        points.clear();


        if (global_cfg.contains("gvar_extended"))
        {
            if (global_cfg["gvar_extended"].contains("preview_misc_text"))
                misc_preview_text = global_cfg["gvar_extended"]["preview_misc_text"].get<std::string>();
            else
                misc_preview_text = "SatDump | GVAR";

            if (global_cfg["gvar_extended"].contains("crop"))
            {
                for (int i = 0; i < (int)global_cfg["gvar_extended"]["crop"].size(); i++)
                {
                    float x1,x2,y1,y2;
                    std::string name,layer;
                    bool map,date;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("x1")) x1=global_cfg["gvar_extended"]["crop"][i]["x1"].get<float>(); else x1=0;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("y1")) y1=global_cfg["gvar_extended"]["crop"][i]["y1"].get<float>(); else y1=0;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("x2")) x2=global_cfg["gvar_extended"]["crop"][i]["x2"].get<float>(); else x2=1;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("y2")) y2=global_cfg["gvar_extended"]["crop"][i]["y2"].get<float>(); else y2=1;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("layer")) layer=global_cfg["gvar_extended"]["crop"][i]["layer"].get<std::string>(); else continue;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("name")) name=global_cfg["gvar_extended"]["crop"][i]["name"].get<std::string>(); else continue;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("map_overlay")) map=global_cfg["gvar_extended"]["crop"][i]["map_overlay"].get<bool>(); else map=false;
                    if(global_cfg["gvar_extended"]["crop"][i].contains("date_in_name")) date=global_cfg["gvar_extended"]["crop"][i]["date_in_name"].get<bool>(); else date=false;

                    crops.push_back({x1,y1,x2,y2,name,layer,map,date});
                }
            }
            else
            {//No crops defined, load defaults
                    crops.push_back({500, 50, 500 + 1560, 50 + 890,"europe_IR","4",true,false});
                    crops.push_back({1348, 240, 1348 + 5928, 240 + 4120,"europe_VIS","1",true,false});
                    crops.push_back({1348, 240, 1348 + 5928, 240 + 4120,"europe","FC",false,false});
            }
            
            if (global_cfg["gvar_extended"].contains("composite"))
            {
                for (int comp = 0; comp < (int)global_cfg["gvar_extended"]["composite"].size(); comp++)
                {
                    std::vector<images_t> images;
                    std::string name;
                    bool date;
                    uint16_t width,height;
                    if(global_cfg["gvar_extended"]["composite"][comp].contains("name")) name=global_cfg["gvar_extended"]["composite"][comp]["name"].get<std::string>(); else continue;
                    if(global_cfg["gvar_extended"]["composite"][comp].contains("date_in_name")) date=global_cfg["gvar_extended"]["composite"][comp]["date_in_name"].get<bool>(); else date=false;
                    if(global_cfg["gvar_extended"]["composite"][comp].contains("width")) width=global_cfg["gvar_extended"]["composite"][comp]["width"].get<float>(); else continue;
                    if(global_cfg["gvar_extended"]["composite"][comp].contains("height")) height=global_cfg["gvar_extended"]["composite"][comp]["height"].get<float>(); else continue;

                    for ( int image =0; image < (int)global_cfg["gvar_extended"]["composite"][comp]["images"].size();image++)
                    {
                        float x1,x2,y1,y2;
                        uint16_t x,y,width2,height2;
                        std::string layer;
                        bool map;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"].contains("x1")) x1=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"]["x1"].get<float>(); else x1=0;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"].contains("y1")) y1=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"]["y1"].get<float>(); else y1=0;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"].contains("x2")) x2=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"]["x2"].get<float>(); else x2=1;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"].contains("y2")) y2=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["crop"]["y2"].get<float>(); else y2=1;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image].contains("layer")) layer=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["layer"].get<std::string>(); else continue;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image].contains("map_overlay")) map=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["map_overlay"].get<bool>(); else map=false;

                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image].contains("x")) x=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["x"].get<u_int16_t>(); else continue;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image].contains("y")) y=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["y"].get<u_int16_t>(); else continue;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image].contains("width")) width2=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["width"].get<u_int16_t>(); else continue;
                        if(global_cfg["gvar_extended"]["composite"][comp]["images"][image].contains("height")) height2=global_cfg["gvar_extended"]["composite"][comp]["images"][image]["height"].get<u_int16_t>(); else continue;
                        images_t imageObj={{x1,y1,x2,y2,"",layer,map,false},x,y,width2,height2};

                        images.push_back(imageObj);
                    }
                    composites.push_back({name,date,width, height,images});
                }
            }
            else
            {//No preview defined, load default

                composites.push_back({"preview",false,1300, 948,{
                    {{0,0,1,1,"","1",false,false},   0,  0,1040, 948},
                    {{0,0,1,1,"","2",false,false},1040,  0, 260, 237},
                    {{0,0,1,1,"","3",false,false},1040,237, 260, 237},
                    {{0,0,1,1,"","4",false,false},1040,474, 260, 237},
                    {{0,0,1,1,"","5",false,false},1040,711, 260, 237},
                }});
            }
            
            if (global_cfg["gvar_extended"].contains("temperature_points"))
            {
                points.clear();
                for (int i = 0; i < (int)global_cfg["gvar_extended"]["temperature_points"].size(); i++)
                {
                    points.push_back({global_cfg["gvar_extended"]["temperature_points"][i]["lat"].get<float>(),
                                      global_cfg["gvar_extended"]["temperature_points"][i]["lon"].get<float>(),
                                      global_cfg["gvar_extended"]["temperature_points"][i]["radius"].get<float>()});
                    names.push_back(global_cfg["gvar_extended"]["temperature_points"][i]["name"].get<std::string>());
                }
            }
        }
    }

    static std::string getGvarFilename(int sat_number, std::tm *timeReadable, std::string channel)
    {
        std::string utc_filename = "G" + std::to_string(sat_number) + "_" + channel + "_" +                                                                     // Satellite name and channel
                                    std::to_string(timeReadable->tm_year + 1900) +                                                                               // Year yyyy
                                    (timeReadable->tm_mon + 1 > 9 ? std::to_string(timeReadable->tm_mon + 1) : "0" + std::to_string(timeReadable->tm_mon + 1)) + // Month MM
                                    (timeReadable->tm_mday > 9 ? std::to_string(timeReadable->tm_mday) : "0" + std::to_string(timeReadable->tm_mday)) + "T" +    // Day dd
                                    (timeReadable->tm_hour > 9 ? std::to_string(timeReadable->tm_hour) : "0" + std::to_string(timeReadable->tm_hour)) +          // Hour HH
                                    (timeReadable->tm_min > 9 ? std::to_string(timeReadable->tm_min) : "0" + std::to_string(timeReadable->tm_min)) +             // Minutes mm
                                    (timeReadable->tm_sec > 9 ? std::to_string(timeReadable->tm_sec) : "0" + std::to_string(timeReadable->tm_sec)) + "Z";        // Seconds ss
        return utc_filename;
    }
    static void gvarSaveChannelImagesHandler(const goes::gvar::events::GVARSaveChannelImagesEvent &evt)
    {
        
        for(int i=0;i<composites.size();i++)
        {
            composite_t comp = composites[i];

            logger->info("Preview... " + comp.name +".png");
            image::Image<uint8_t> preview(comp.width, comp.height, 1);
            image::Image<uint8_t> previewImage;

            // Preview generation
            for(int sub=0;sub<comp.images.size();sub++)
            {
                images_t subImage=comp.images[sub];

                image::Image<uint8_t> channel_8bit(subImage.width, subImage.height, 1);

                image::Image<uint16_t> resized;
                if     (subImage.crop.channel=="1") resized = cropVIS(evt.images.image5);
                else if(subImage.crop.channel=="2") resized = cropIR(evt.images.image1);
                else if(subImage.crop.channel=="3") resized = cropIR(evt.images.image2);
                else if(subImage.crop.channel=="4") resized = cropIR(evt.images.image3);
                else if(subImage.crop.channel=="5") resized = cropIR(evt.images.image4);
                else continue;
                
                resized.resize(subImage.width/(subImage.crop.x2-subImage.crop.x1), subImage.height/(subImage.crop.y2-subImage.crop.y1));

                //Map?
                if(subImage.crop.include_map) drawMapOverlay(evt.images.sat_number, evt.timeUTC, resized);

                //Crop if required
                if(subImage.crop.x1!=0 || subImage.crop.y1!=0 || subImage.crop.x2!=1 || subImage.crop.y2!=1)
                {//Crop configured.
                    if(subImage.crop.x1>1 || subImage.crop.y1>1 || subImage.crop.x2>1 || subImage.crop.y2>1)
                        //Crop set by pixels
                        resized.crop(subImage.crop.x1, subImage.crop.y1, subImage.crop.x2, subImage.crop.y2);
                    else
                        //Crop set by percentages
                        resized.crop(resized.width()*subImage.crop.x1, resized.height()*subImage.crop.y1, resized.width()*subImage.crop.x2, resized.height()*subImage.crop.y2);
                }


                for (int i = 0; i < (int)channel_8bit.size(); i++)
                {
                    channel_8bit[i] = resized[i] >> 8;//Divide by 256. Shift = faster?
                }
                
                channel_8bit.simple_despeckle();

                preview.draw_image(0, channel_8bit, subImage.x, subImage.y);
            }

            // Overlay the preview
            {
                std::string sat_name = evt.images.sat_number == 13 ? "EWS-G1 / GOES-13" : ("GOES-" + std::to_string(evt.images.sat_number));
                std::string date_time = (evt.timeReadable->tm_mday > 9 ? std::to_string(evt.timeReadable->tm_mday) : "0" + std::to_string(evt.timeReadable->tm_mday)) + '/' +
                                        (evt.timeReadable->tm_mon + 1 > 9 ? std::to_string(evt.timeReadable->tm_mon + 1) : "0" + std::to_string(evt.timeReadable->tm_mon + 1)) + '/' +
                                        std::to_string(evt.timeReadable->tm_year + 1900) + ' ' +
                                        (evt.timeReadable->tm_hour > 9 ? std::to_string(evt.timeReadable->tm_hour) : "0" + std::to_string(evt.timeReadable->tm_hour)) + ':' +
                                        (evt.timeReadable->tm_min > 9 ? std::to_string(evt.timeReadable->tm_min) : "0" + std::to_string(evt.timeReadable->tm_min)) + " UTC";

                int offsetX, offsetY, bar_height;

                //set ratios for calculating bar size
                float bar_ratio = 0.02;


                bar_height = preview.width() * bar_ratio;
                offsetX = 5; //preview.width() * offsetXratio;
                offsetY = 1; //preview.width() * offsetYratio;

                unsigned char color = 255;

                image::Image<uint8_t> imgtext = image::generate_text_image(sat_name.c_str(), &color, bar_height, offsetX, offsetY); 
                image::Image<uint8_t>imgtext1 = image::generate_text_image(date_time.c_str(), &color, bar_height, offsetX, offsetY); 
                image::Image<uint8_t>imgtext2 = image::generate_text_image(misc_preview_text.c_str(), &color, bar_height, offsetX, offsetY); 

                previewImage = image::Image<uint8_t>(preview.width(), preview.height() + 2 * bar_height, 1);
                previewImage.fill(0);

                previewImage.draw_image(0, imgtext, 0, 0);
                previewImage.draw_image(0, imgtext1, previewImage.width() - imgtext1.width(), 0);
                previewImage.draw_image(0, imgtext2, 0, bar_height + preview.height());
                previewImage.draw_image(0, preview, 0, bar_height);
            }

            previewImage.save_png(std::string(evt.directory + "/" + comp.name +".png").c_str());
        }

        //calibrated temperature measurement based on NOAA LUTs (https://www.ospo.noaa.gov/Operations/GOES/calibration/gvar-conversion.html)
        if (evt.images.image1.width() == 5206 || evt.images.image1.width() == 5209)
        {
            std::string filename = "goes/gvar/goes" + std::to_string(evt.images.sat_number) + "_gvar_lut.txt";
            if (resources::resourceExists(filename) && points.size() > 0)
            {
                std::ifstream input(resources::getResourcePath(filename).c_str());
                std::array<std::array<float, 1024>, 4> LUTs = readLUTValues(input);
                input.close();

                std::ofstream output(evt.directory + "/temperatures.txt");
                logger->info("Temperatures... temperatures.txt");

                image::Image<uint16_t> im1 = cropIR(evt.images.image1);
                image::Image<uint16_t> im2 = cropIR(evt.images.image2);
                image::Image<uint16_t> im3 = cropIR(evt.images.image3);
                image::Image<uint16_t> im4 = cropIR(evt.images.image4);
                std::array<image::Image<uint16_t>, 4> channels = {im1, im2, im3, im4};

                geodetic::projection::GEOProjector proj(61.5, 35782.466981, 18990, 18956, 1.1737, 1.1753, 0, -40, 1);

                for (int j = 0; j < (int)points.size(); j++)
                {
                    int x, y;
                    proj.forward(points[j][1], points[j][0], x, y);
                    x /= 4;
                    y /= 4;

                    output << "Temperature measurements for point [" + std::to_string(x) + ", " + std::to_string(y) + "] (" + names[j] + ") with r = " + std::to_string(points[j][2]) << '\n'
                           << '\n';

                    logger->info("Temperature measurements for point [" + std::to_string(x) + ", " + std::to_string(y) + "] (" + names[j] + ") with r = " + std::to_string(points[j][2]));
                    for (int i = 0; i < 4; i++)
                    {
                        output << "    Channel " + std::to_string(i + 2) + ":     " << FIXED_FLOAT(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6])
                               << " K    (";
                        if (LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15 < 10)
                        {
                            if (LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15 <= -10)
                            {
                                output << FIXED_FLOAT(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15);
                            }
                            else if (LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15 > 0)
                            {
                                output << "  " << FIXED_FLOAT(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15);
                            }
                            else
                            {
                                output << " " << FIXED_FLOAT(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15);
                            }
                        }
                        else
                        {
                            output << " " << FIXED_FLOAT(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15);
                        }
                        output << " °C)";
                        output << '\n';
                        logger->info("channel " + std::to_string(i + 2) + ":     " +
                                     std::to_string(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6]) +
                                     " K    (" + std::to_string(LUTs[i][getAVG(channels[i], x, y, points[j][2]) >> 6] - 273.15) + " °C)");
                    }
                    output << '\n'
                           << '\n';
                }
                output.close();
            }
            else
            {
                logger->warn("goes/gvar/goes" + std::to_string(evt.images.sat_number) + "_gvar_lut.txt LUT is missing! Temperature measurement will not be performed.");
            }
        }
        else
        {
            logger->info("Image is not a FD, temperature measurement will not be performed.");
        }

        logger->info("Generating mapped crops..");
        //mapped crops of europe. IR and VIS
        for (int j = 0; j < (int)crops.size(); j++)
        {
            std::string filename;
            if(crops[j].include_date) filename = getGvarFilename(evt.images.sat_number,evt.timeReadable,crops[j].channel)+"_"; else filename="";
            image::Image<uint16_t> mapProj;
            if(crops[j].channel=="1") mapProj = cropVIS(evt.images.image5);
            else if(crops[j].channel=="2") mapProj = cropIR(evt.images.image1);
            else if(crops[j].channel=="3") mapProj = cropIR(evt.images.image2);
            else if(crops[j].channel=="4") mapProj = cropIR(evt.images.image3);
            else if(crops[j].channel=="5") mapProj = cropIR(evt.images.image4);
            else continue;

            if(crops[j].include_map) drawMapOverlay(evt.images.sat_number, evt.timeUTC, mapProj);
            
            if(crops[j].x1>1 || crops[j].y1>1 || crops[j].x2>1 || crops[j].y2>1)
                //Crop set by pixels
                mapProj.crop(crops[j].x1, crops[j].y1, crops[j].x2, crops[j].y2);
            else
                //Crop set by percentages
                mapProj.crop(mapProj.width()*crops[j].x1, mapProj.height()*crops[j].y1, mapProj.width()*crops[j].x2, mapProj.height()*crops[j].y2);
            
            logger->info((std::string)((crops[j].channel=="1")?"VIS":"IR") + " crop.. " + filename + crops[j].name + ".png");
            mapProj.to8bits().save_png(std::string(evt.directory + "/" + filename + crops[j].name + ".png").c_str());

            mapProj.clear();
        }
    }

    static void gvarSaveFalseColorHandler(const goes::gvar::events::GVARSaveFCImageEvent &evt)
    {
        if (evt.sat_number == 13)
        {
            for (int j = 0; j < (int)crops.size(); j++)
            {
                if(crops[j].channel=="FC")
                {
                    std::string filename;
                    if(crops[j].include_date) filename = getGvarFilename(evt.sat_number,evt.timeReadable,crops[j].channel)+"_"; else filename="";
                    logger->info("crop... " + filename + crops[j].name + ".png");
                    image::Image<uint8_t> crop = cropFC(evt.false_color_image);
                    if(crops[j].include_map) drawMapOverlay(evt.sat_number, evt.timeUTC, crop);

                    if(crops[j].x1>1 || crops[j].y1>1 || crops[j].x2>1 || crops[j].y2>1)
                        //Crop set by pixels
                        crop.crop(crops[j].x1, crops[j].y1, crops[j].x2, crops[j].y2);
                    else
                        //Crop set by percentages
                        crop.crop(crop.width()*crops[j].x1, crop.height()*crops[j].y1, crop.width()*crops[j].x2, crop.height()*crops[j].y2);

                    crop.save_png(std::string(evt.directory + "/"+ filename +crops[j].name + ".png").c_str());
                }
            }
        }
    }

    static std::array<std::array<float, 1024>, 4> readLUTValues(std::ifstream &LUT)
    {
        std::array<std::array<float, 1024>, 4> values;
        std::string tmp;
        //skip first 7 lines
        for (int i = 0; i < 7; i++)
        {
            std::getline(LUT, tmp);
        }
        //read LUTs
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 1024; j++)
            {
                std::getline(LUT, tmp);
                values[i][j] = std::stof(tmp.substr(39, 7));
            }
            if (i != 3)
            {
                //skip det2 for first 3 channels (no det2 for ch6)
                for (int j = 0; j < 1030; j++)
                {
                    std::getline(LUT, tmp);
                }
            }
        }
        return values;
    }

    static unsigned short getAVG(image::Image<uint16_t> &image, int x, int y, int r)
    {
        uint64_t sum = 0;
        for (int i = 0; i < std::pow(r * 2 + 1, 2); i++)
        {
            sum += image[(y - r + i / (2 * r + 1))*image.width() + (x - r + i % (2 * r + 1))];
        }
        return sum / std::pow(r * 2 + 1, 2);
    }

        static image::Image<uint16_t> cropIR(image::Image<uint16_t> input)
    {
        image::Image<uint16_t> output(4749, input.height(), 1);
        if (input.width() == 5206)
        {
            output.draw_image(0, input, 0, 0);
        }
        else if (input.width() == 5209)
        {
            output.draw_image(0, input, -463, 0);
        }
        else
        {
            logger->warn("Wrong IR image size (" + std::to_string(input.width()) + "), it will not be cropped");
            return input;
        }
        return output;
    }

    static image::Image<uint16_t> cropVIS(image::Image<uint16_t> input)
    {
        image::Image<uint16_t> output(18990, input.height(), 1);
        if (input.width() == 20824)
        {
            output.draw_image(0, input, 0, 0);
        }
        else if (input.width() == 20836)
        {
            output.draw_image(0, input, -1852, 0);
        }
        else
        {
            logger->warn("Wrong VIS image size (" + std::to_string(input.width()) + "), it will not be cropped");
            return input;
        }
        return output;
    }
    static image::Image<uint8_t> cropFC(image::Image<uint8_t> input)
    {
        image::Image<uint8_t> output(18990, input.height(), 3);
        if (input.width() == 20824)
        {
            output.draw_image(0, input, 0, 0);
        }
        else if (input.width() == 20836)
        {
            output.draw_image(0, input, -1852, 0);
        }
        else
        {
            logger->warn("Wrong FC image size (" + std::to_string(input.width()) + "), it will not be cropped");
            return input;
        }
        return output;
    }

    static int getNORADFromSatNumber(int number)
    {
        if (number == 13)
            return 29155;
        else if (number == 14)
            return 35491;
        else if (number == 15)
            return 36411;
        else
            return 0;
    };

    static predict_position getSatellitePosition(int number, time_t time)
    {
        tle::TLE goes_tle = tle::getTLEfromNORAD(getNORADFromSatNumber(number));
        predict_orbital_elements_t *goes_object = predict_parse_tle(goes_tle.line1.c_str(), goes_tle.line2.c_str());
        predict_position goes_position;
        predict_orbit(goes_object, &goes_position, predict_to_julian(time));
        predict_destroy_orbital_elements(goes_object);
        return goes_position;
    }

    // Expect cropped IR
    static void drawMapOverlay(int number, time_t time, image::Image<uint16_t> &image)
    {
        geodetic::projection::GEOProjector proj(61.5, 35782.466981, 18990, 18956, 1.1737, 1.1753, 0, -40, 1);

        unsigned short color[3] = {65535, 65535, 65535};

        map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")}, image, color, [&proj, &image](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
                                       {
                                           int image_x, image_y;
                                           proj.forward(lon, lat, image_x, image_y);
                                           if (image.width() == 18990)
                                           {
                                               return {image_x, image_y};
                                           }
                                           else
                                           {
                                               if (image_x == -1 && image_y == -1)
                                               {
                                                   return {-1, -1};
                                               }
                                               else
                                               {
                                                   return {image_x / 4, image_y / 4};
                                               }
                                           }
                                       });

    }
    static void drawMapOverlay(int number, time_t time, image::Image<uint8_t> &image)
    {
        geodetic::projection::GEOProjector proj(61.5, 35782.466981, 18990, 18956, 1.1737, 1.1753, 0, -40, 1);

        uint8_t color[3] = {255, 255, 255};

        map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")}, image, color, [&proj, &image](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
                                       {
                                           int image_x, image_y;
                                           proj.forward(lon, lat, image_x, image_y);
                                           if (image.width() == 18990)
                                           {
                                               return {image_x, image_y};
                                           }
                                           else
                                           {
                                               if (image_x == -1 && image_y == -1)
                                               {
                                                   return {-1, -1};
                                               }
                                               else
                                               {
                                                   return {image_x / 4, image_y / 4};
                                               }
                                           }
                                       });

    }


public:
    std::string getID()
    {
        return "gvar_extended";
    }

    void init()
    {
        satdump::eventBus->register_handler<satdump::SatDumpStartedEvent>(satdumpStartedHandler);
        satdump::eventBus->register_handler<goes::gvar::events::GVARSaveChannelImagesEvent>(gvarSaveChannelImagesHandler);
        satdump::eventBus->register_handler<goes::gvar::events::GVARSaveFCImageEvent>(gvarSaveFalseColorHandler);
    }
};

std::string GVARExtended::misc_preview_text = "SatDump | GVAR";
std::vector<std::array<float, 3>> GVARExtended::points = {{}};
std::vector<std::string> GVARExtended::names = {};
std::vector<crop_t> GVARExtended::crops = {};
std::vector<composite_t> GVARExtended::composites = {};

PLUGIN_LOADER(GVARExtended)
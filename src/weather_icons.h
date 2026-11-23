#pragma once
#include <string>
namespace weathericons {
// 24x24, 1.5px rounded strokes, matching the supplied AQI outlines.
inline std::string Sun(){return "M16 12C16 14.21 14.21 16 12 16C9.79 16 8 14.21 8 12C8 9.79 9.79 8 12 8C14.21 8 16 9.79 16 12Z M12 2V4 M12 20V22 M2 12H4 M20 12H22 M4.9 4.9L6.3 6.3 M17.7 17.7L19.1 19.1 M4.9 19.1L6.3 17.7 M17.7 6.3L19.1 4.9";}
inline std::string Moon(){return "M20.5 14.2C19.3 18.8 14.7 21.6 10.1 20.4C5.5 19.2 2.7 14.6 3.9 10C4.7 6.8 7.3 4.4 10.4 3.8C8.1 7.1 9 11.7 12.3 14C14.8 15.7 18 15.8 20.5 14.2Z";}
inline std::string Cloud(){return "M6.5 16H18C20.2 16 22 14.2 22 12C22 9.8 20.2 8 18 8H17.7C16.9 5.6 14.8 4 12.3 4C9.1 4 6.5 6.4 6.3 9.5C3.9 9.4 2 10.8 2 12.8C2 14.8 3.8 16 6.5 16Z";}
inline std::string Rain(){return " M7 19L6 22 M12 19L11 22 M17 19L16 22";}
inline std::string Snow(){return " M7 19V23 M5.3 20L8.7 22 M5.3 22L8.7 20 M16 19V23 M14.3 20L17.7 22 M14.3 22L17.7 20";}
inline std::string Lightning(){return " M13 15L9 20H13L11 24";}
inline std::string Partial(bool night){return night?"M5 2C3 5 4.4 8.4 7.8 8.8 M2 3V4":"M9 5C9 6.7 7.7 8 6 8C4.3 8 3 6.7 3 5C3 3.3 4.3 2 6 2C7.7 2 9 3.3 9 5Z M1 10L2 9 M11 1L10 2 M1 1L2 2";}
inline std::string Paths(int code){
 if(code==1||code==2)return Sun();
 if(code==33||code==34)return Moon();
 if(code==5||code==37)return (code==37?Moon():Sun())+" M3 18H21 M5 21H19";
 if(code==11)return Cloud()+" M2 19H22 M5 22H19";
 if(code==32)return "M2 8H16C20 8 20 2 16 2C14 2 13 3 13 4 M2 12H20 M2 16H13C17 16 17 22 13 22C11 22 10 21 10 20";
 if(code==30)return Sun()+" M20 17V22 M18 20H22";
 if(code==31)return Snow()+" M12 2V15 M6.4 5.2L17.6 11.8 M6.4 11.8L17.6 5.2 M10 3L12 5L14 3 M10 14L12 12L14 14";
 std::string p=Cloud();bool partial=code==3||code==4||code==13||code==14||code==16||code==17||code==20||code==21||code==23||code==35||code==36||code==38||code==39||code==41||code==43;
 if(partial)p=Partial(code>=33)+p;
 if(code==15||code==16||code==17||code==41||code==42)return p+Lightning();
 if(code==19||code==20||code==21||code==22||code==23||code==43||code==44)return p+Snow();
 if(code==24||code==25||code==26||code==29)return p+" M7 19L6 22 M15 19V23 M13.3 20L16.7 22 M13.3 22L16.7 20";
 if(code==12||code==13||code==14||code==18||code==39||code==40)return p+Rain();
 return p;
}
}

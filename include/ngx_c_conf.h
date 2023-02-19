#ifndef NGX_C_CONF_H
#define NGX_C_CONF_H

class Config{
private:
    Config()=default;
public:
    Config * getInstance();
};


#endif
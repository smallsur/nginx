//
// Created by awen on 23-3-29.
//

#include "ngx_c_conf.h"
#include "gtest/gtest.h"

class Test_conf : public ::testing::Test{
protected:
    void SetUp() override {
        Test::SetUp();
        config = &Config_Nginx::get_instance();
    }

    void TearDown() override {
        Test::TearDown();
    }

    Config_Nginx* config;

};

TEST_F(Test_conf, load){
    EXPECT_EQ(true,config->load("/home/awen/workstation/nginx/nginx.conf"));
}


int main(int argc, char **argv){
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
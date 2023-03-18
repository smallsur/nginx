//
// Created by awen on 23-3-16.
//

#ifndef NGINX_SOLUTION_H
#define NGINX_SOLUTION_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
using namespace std;


class Solution {
public:
    int search(vector<int>& nums, int target) {
        int start=0,end=nums.size()-1;
        while (start<=end){
            int mid = (start - end)/2 + end;
            if(nums[mid]<target){
                start = mid + 1;
            } else if(nums[mid]>target){
                end = mid - 1;
            } else{
                return mid;
            }
        }
        return -1;
    }
    bool isBadVersion(int version);
    int firstBadVersion(int n) {
        int start=1,end=n;
        while (start < end - 1){
            int mid = (start - end)/2 + end;
            if(isBadVersion(mid)){
                end = mid;
            } else{
                start = mid + 1;
            }
        }
        start = isBadVersion(start)?start:start+1;
        return start;
    }
    vector<int> sortedSquares(vector<int>& nums) {
        vector<int> ans;
        int left=0,right=nums.size()-1;
        while (left <= right){
            int l = ::pow(nums[left],2);
            int r = ::pow(nums[right],2);
            if(l<r){
                ans.push_back(r);
                right--;
            } else{
                ans.push_back(l);
                left++;
            }
        }
        ::reverse(ans.begin(),ans.end());
        return ans;
    }
    void moveZeroes(vector<int>& nums) {
        int n = nums.size();
        int start = 0,end=0;
        while (start<n){
            if(nums[start]==0){
                while (end<n && nums[end]==0){
                    end++;
                }
            } else{
                start++;
                end++;
                continue;
            }
            while (end<n && nums[end]!=0){
                int temp = nums[start];
                nums[start]=nums[end];
                nums[end]=temp;
                start++;
                end++;
            }
        }
    }

    vector<int> twoSum(vector<int>& numbers, int target) {
        int left = 0;
        int right = numbers.size()-1;
        vector<int> ans{2,-1};
        int temp = -1;
        while (left< right){
            temp = numbers[left]+numbers[right];
            if(temp>target){
                right--;
            } else if(temp<target){
                left++;
            } else{
                ans[0]=left+1;
                ans[1]=right+1;
            }
        }
        return ans;
    }
    int search(vector<int>&nums,int left,int right, int target){
        int start=left,end=right;
        while (start<=end){
            int mid = (start - end)/2 + end;
            if(nums[mid]<target){
                start = mid + 1;
            } else if(nums[mid]>target){
                end = mid - 1;
            } else{
                return mid;
            }
        }
        return -1;
    }



};



#endif //NGINX_SOLUTION_H

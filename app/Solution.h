//
// Created by awen on 23-3-16.
//

#ifndef NGINX_SOLUTION_H
#define NGINX_SOLUTION_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <map>
#include <set>
#include <functional>
using namespace std;

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};
class Solution {
public:
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
    string findLexSmallestString(string s, int a, int b) {
        int n = s.size();
        string ans;
        for (int i = 0; i < 10; ++i) {
            string s1 = s;
            for (int j = 1; j < n; j += 2) {
                s1[j] = (s1[i]-'0'+a)%10 + '0';
            }

        }
    }
    int findTargetSumWays(vector<int>& nums, int target) {
        int n =nums.size();
        int ans = 0;
        int cnt = 0;
        map<int,set<int>> dp;
        function<void(int)> dfs = [&](int i){
            if(i==n) {
                if(cnt==target){
                    ans++;
                    return ;
                }
                return;
            }
            cnt -= nums[i];
            dfs(i+1);
            cnt += nums[i];
            
            cnt += nums[i];
            dfs(i+1);
            cnt -= nums[i];
        };
        dfs(0);
        return ans;
    }
    ListNode* removeNthFromEnd(ListNode* head, int n) {
        ListNode* left = head;
        ListNode* right = head;
        for (int i = 0; i < n; ++i) {
            right = right->next;
        }
        while (right->next!= nullptr){
            left = left->next;
            right = right->next;
        }
        if(n==1){
            left->next= nullptr;
            delete right;
            return head;
        }
        ListNode* tmp = left->next;
        left->val = left->next->val;
        left->next = left->next->next;
        delete tmp;
        return head;
    }

};



#endif //NGINX_SOLUTION_H

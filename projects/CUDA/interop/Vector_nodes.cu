#include "Vector.hpp"
#include "zensim/cuda/execution/ExecutionPolicy.cuh"
#include "zensim/ZpcFunctional.hpp"
#include <fmt/core.h>
#include <tuple>
#include <variant>
#include <zeno/zeno.h>

namespace zeno
{
    struct MakeZsVector : INode
    {
        void apply() override
        {
            // TODO
            auto input_size = get_input2<int>("size");
            auto input_memsrc = get_input2<std::string>("memsrc");
            auto intput_devid = get_input2<int>("dev_id");
            // auto input_virtual = get_input2<bool>("virtual");
            auto intput_elem_type = get_input2<std::string>("elem_type");

            zs::memsrc_e memsrc;
            if (input_memsrc == "host")
                memsrc = zs::memsrc_e::host;
            else if (input_memsrc == "device")
                memsrc = zs::memsrc_e::device;
            else
                memsrc = zs::memsrc_e::um;

#define MAKE_VECTOR_OBJ_T(T)                                                                   \
    if (intput_elem_type == #T)                                                                \
    {                                                                                          \
        auto allocator = zs::get_memory_source(memsrc, static_cast<zs::ProcID>(intput_devid)); \
        vectorObj->set(zs::Vector<T, zs::ZSPmrAllocator<false>>{allocator, 0});                \
    }

            auto vectorObj = std::make_shared<VectorViewLiteObject>();
            MAKE_VECTOR_OBJ_T(int)
            MAKE_VECTOR_OBJ_T(float)
            MAKE_VECTOR_OBJ_T(double)
            std::visit([input_size](auto &vec)
                       { vec.resize(input_size); },
                       vectorObj->value);

            set_output("ZsVector", std::move(vectorObj));
        }
    };

    //  memsrc, size, elem_type, dev_id, virtual
    ZENDEFNODE(MakeZsVector, {
                                 {{"int", "size", "0"},
                                  {"enum host device um", "memsrc", "device"},
                                  {"int", "dev_id", "0"},
                                  //   {"bool", "virtual", "false"},
                                  {"enum float int double", "elem_type", "float"}},
                                 {"ZsVector"},
                                 {},
                                 {"PyZFX"},
                             });

    struct ReduceZsVector : INode
    {
        void apply() override
        {
            auto vectorObj = get_input<VectorViewLiteObject>("ZsVector");
            auto opStr = get_input2<std::string>("op"); 
            auto &vector = vectorObj->value;

            float result; 
            std::visit([&result, &opStr](auto &vector){
                auto pol = zs::cuda_exec();
                using vector_t = RM_CVREF_T(vector); 
                using val_t = typename vector_t::value_type; 
                zs::Vector<val_t> res{1, zs::memsrc_e::device, 0};
                auto host_vector = vector.clone({zs::memsrc_e::host, -1}); 
                // NOTE: for debugging 
                fmt::print("vector[0] = {}, opStr = {}\n", host_vector[0], opStr); 
                if (opStr == "add")
                    zs::reduce(pol, std::begin(vector), std::end(vector), std::begin(res), 
                        static_cast<val_t>(0), zs::plus<val_t>{});
                else if (opStr == "max")
                    zs::reduce(pol, std::begin(vector), std::end(vector), std::begin(res), 
                        zs::limits<val_t>::min(), zs::getmax<val_t>{});
                else 
                    zs::reduce(pol, std::begin(vector), std::end(vector), std::begin(res), 
                        zs::limits<val_t>::max(), zs::getmin<val_t>{});
                // NOTE: for debugging 
                fmt::print("res.getVal(): {}\n", res.getVal()); 
                result = static_cast<float>(res.getVal()); 
            }, vector); 
            // NOTE: for debugging 
            fmt::print("reduce result: {}\n", result); 
            set_output2("result", result); 
        }
    }; 

    ZENDEFNODE(ReduceZsVector, {
                                   {"ZsVector",
                                    {"enum add max min", "op", "add"}},
                                   {"result"},
                                   {},
                                   {"PyZFX"},
                               });
}
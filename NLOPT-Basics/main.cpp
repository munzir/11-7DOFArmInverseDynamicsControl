#include <iostream>
#include <nlopt.hpp>
#include <vector>

typedef struct{
    double a, b;
}my_constraint_data;


double myfunc(unsigned n, const double *x, double *grad, void *myfunc_data){
    if(grad){
        grad[0] = 0.0;
        grad[1] = 2*(x[1]);
    }
    return (x[1]*x[1]);
}



double myconstraint(unsigned n, const double *x, double *grad, void *data){
    my_constraint_data *d = (my_constraint_data *) data;
    double a = d->a;
    double b = d->b;
    if(grad){
        grad[0] = a;
        grad[1] = -1;
    }
    return ((a*x[0]+b) - x[1]);
}





int main() {
    std::cout << "Hello, World!" << std::endl;
    nlopt::opt my_object(nlopt::LD_MMA,2);
    std::vector<double> lb(2);
    lb[0] = -HUGE_VAL;
    lb[1] = 0;
    my_object.set_lower_bounds(lb);
    my_object.set_min_objective(myfunc, NULL);
    my_constraint_data data[2] = {{2,0}, {-1,1}};
    my_object.add_inequality_constraint(myconstraint, &data[0], 1e-8);
    my_object.add_inequality_constraint(myconstraint, &data[1], 1e-8);
    my_object.set_xtol_rel(1e-4);
    std::vector<double> x(2);
    x[0] = 1.1;
    x[1] = 5.67;
    double minf;

    try {
        nlopt::result result = my_object.optimize(x, minf);
        std::cout<<"Found minimum of f at ("<<x[0]<<","<<x[1] <<") = "<<minf<<std::endl;
    }
    catch(std::exception &e){
        std::cout<<"NLOPT Failed"<<e.what() <<std::endl;
    }


    return 0;

}
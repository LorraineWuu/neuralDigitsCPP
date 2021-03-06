#include "network.h"
using namespace NeuralNetwork;

Network::Network(const VectorXi & layer_sizes)
{
	std::default_random_engine gen(5336);
	std::normal_distribution<double> dist(0.,1.);

	// TODO initialize matricies with vector in order to use nicer c++ routines for initialization than the loops?
	number_of_layers = layer_sizes.size();
	for (int i = 1; i < number_of_layers; ++i)
	{
		weights.push_back(MatrixXd::Zero(layer_sizes(i),layer_sizes(i-1)));
		biases.push_back(VectorXd::Zero(layer_sizes(i)));
	}
	for (int i = 0; i < number_of_layers-1; ++i)
	{
		for (int j = 0; j < weights[i].rows(); ++j)
			for (int k = 0; k < weights[i].cols(); ++k)
				weights[i](j,k) = dist(gen);
		for (int j = 0; j < biases[i].rows(); ++j)
			biases[i](j) = dist(gen);
	}
}

VectorXd Network::feed_forward(VectorXd&& x)
{
	for (int i = 0; i < number_of_layers-1; ++i)
		x = sigmoid((weights[i] * x).eval() + biases[i]);
	return x;
}

double Network::SGD(const Data & train_data, const Data & test_data,
                    int epochs, int mini_batch_size, double learning_rate)
{
	const int dimension = train_data.examples.rows();
	const int num_examples = train_data.examples.cols();
	const int num_batches = num_examples/mini_batch_size;
	Data mini_batch;
	mini_batch.examples = MatrixXd::Zero(dimension, mini_batch_size);
	mini_batch.labels = VectorXi::Zero(mini_batch_size);

	// fill random_indices with increasing numbers starting from 0
	vector<int> random_indices(num_examples);
	int n = 0; std::generate(random_indices.begin(), random_indices.end(), [&n]{ return n++; });
	for (int j = 0; j < epochs; ++j)
	{
		std::random_shuffle(random_indices.begin(), random_indices.end());
		for (int i = 0; i < num_batches; ++i)
		{
			// Make a minibatch
			for (int k = 0; k < mini_batch_size; ++k)
			{
				mini_batch.examples.col(k) = train_data.examples.col(random_indices[k+mini_batch_size*i]);
				mini_batch.labels(k) = train_data.labels(random_indices[k+mini_batch_size*i]);
			}
			train_for_mini_batch(mini_batch, learning_rate);
		}
		cout << "Epoch " << j << " : " << evaluate(test_data) << "% accuracy" << endl << endl;
	}
	return evaluate(test_data);
}

void Network::train_for_mini_batch(const Data & mini_batch, double learning_rate)
{
	vector<MatrixXd> nabla_w(number_of_layers-1);
	vector<VectorXd> nabla_b(number_of_layers-1);
	for (int i = 0; i < number_of_layers-1; ++i)
	{
		nabla_w[i] = MatrixXd::Zero(weights[i].rows(), weights[i].cols());
		nabla_b[i] = VectorXd::Zero(biases[i].rows());
	}
	const int mini_batch_size = mini_batch.labels.rows();
	for (int i = 0; i < mini_batch_size; ++i)
		backprop(mini_batch.examples.col(i), mini_batch.labels(i), nabla_w, nabla_b);
	for (int i = 0; i < number_of_layers-1; ++i)
	{
		weights[i] -= (learning_rate*(nabla_w[i]/double(mini_batch_size)));
		biases[i] -= (learning_rate*(nabla_b[i]/double(mini_batch_size)));
	}
}

void Network::backprop(const VectorXd & x, int y, vector<MatrixXd>& nabla_w, vector<VectorXd>& nabla_b)
{
	vector<VectorXd> activations = {x};
	for (int i = 1; i < number_of_layers; ++i)
		   activations.push_back(sigmoid(weights[i-1]*activations[i-1] + biases[i-1]));
	VectorXd delta = cost_derivative(activations[activations.size()-1], y).array()*activations[activations.size()-1].array()*(1.0-activations[activations.size()-1].array());
	nabla_w[nabla_w.size() -1] += delta * activations[activations.size()-2].transpose();
	nabla_b[nabla_b.size() -1] += delta;
	for (int l = 2; l < number_of_layers; ++l)
	{
		delta = (weights[weights.size() -l+1].transpose() * delta).eval().array()*activations[activations.size()-l].array()*(1.0-activations[activations.size()-l].array());
		nabla_w[nabla_w.size() -l] += delta * activations[activations.size() -l-1].transpose();
		nabla_b[nabla_b.size() -l] += delta;
	}
	// VectorXd activation = x;
	// vector<VectorXd> activations = {x};
	// VectorXd z;
	// vector<VectorXd> zs;
	// for (int i = 0; i < number_of_layers-1; ++i)
	// {
	// 	z = (weights[i]*activation + biases[i]).eval();
	// 	zs.push_back(z);
	// 	activation = sigmoid(z);
	// 	activations.push_back(activation);
	// }
	// VectorXd delta = cost_derivative(activations[activations.size()-1], y).array() * Dsigmoid(zs[zs.size()-1]).array();
	// // VectorXd delta = cost_derivative(activations[activations.size()-1], y).array() * activations[activations.size()-1].array()*(1.0-activations[activations.size()-1].array());
	// nabla_w[nabla_w.size() -1] += delta * activations[activations.size()-2].transpose();
	// nabla_b[nabla_b.size() -1] += delta;
	// VectorXd sp;
	// for (int l = 2; l < number_of_layers; ++l)
	// {
	// 	// z = zs[zs.size()-l];
	// 	// sp = Dsigmoid(z);
	// 	sp = activations[activations.size()-l].array()*(1.0-activations[activations.size()-l].array());
	// 	delta = ((weights[weights.size() -l+1].transpose() * delta).array()*sp.array()).eval();
	// 	nabla_w[nabla_w.size() -l] += delta * activations[activations.size() -l-1].transpose();
	// 	nabla_b[nabla_b.size() -l] += delta;
	// }
}

double Network::evaluate(const Data & test_data)
{
	const int n = test_data.examples.cols();
	vector<int> test_results(n);
	int num_correctly_labeled = 0;
	// This simplification makes the code slower.
	// if (argmax(feed_forward(test_data.examples.col(i))) == test_data.labels(i))
	// 	num_correctly_labeled++;
	for (int i = 0; i < n; ++i)
		test_results[i] = argmax(feed_forward(test_data.examples.col(i)));
	for (int i = 0 ; i < n; ++i)
		if (test_results[i] == test_data.labels(i))
			num_correctly_labeled++;
	return num_correctly_labeled/double(n)*100.;
}

VectorXd Network::cost_derivative(VectorXd output_activations, int y)
{
	// When y is a vector of zeros except for a 1 at the index of the true label
	// return output_activations - y;

	output_activations(y) -= 1.;
	return output_activations;
}

int Network::argmax(const VectorXd & v)
{
	int x;
	(void)v.maxCoeff(&x);
	return x;
}

VectorXd Network::sigmoid(const VectorXd & z)
{
	return 1./(1.+(-z.array()).exp());
}

VectorXd Network::Dsigmoid(const VectorXd & z)
{
	const VectorXd sz = Network::sigmoid(z);
	return sz.array()*(1.0-sz.array());
}

//--- Code for converting module to work with matricies of train examples instead of just single vectors of examples
//--- search eigen partial reduction
// VectorXd Network::feed_forward(MatrixXd&& X)
// {
// 	for (int i = 0; i < number_of_layers-1; ++i)
// 		X = sigmoid((weights[i] * X).colwise() + biases[i]);
// 	return X;
// }

// void Network::backprop(MatrixXd x, int y, vector<MatrixXd>& nabla_w, vector<VectorXd>& nabla_b)
// Ooo
// //add v to each column of m
// mat.colwise() += v;

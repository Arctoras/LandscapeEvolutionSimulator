using System;
using UnityEngine;
using UnityEngine.Windows;

public class TerrainSimulator
{
    ComputeShader simulator;

    bool readA = true;
    RenderTexture texA = null;
    RenderTexture texB = null;

    public Texture states { get { return readA ? texA : texB; } }

    int generationKernel = -1;
    int simulationKernel = -1;

    private float[] simulationParameters = new float[8];
    public float uplift { get { return simulationParameters[0]; } set { simulationParameters[0] = value; simulator.SetFloat("u", simulationParameters[0]); } }
    public float creepSpeed { get { return simulationParameters[1]; } set { simulationParameters[1] = value; simulator.SetFloat("c", simulationParameters[1]); } }
    public float rain { get { return simulationParameters[2]; } set { simulationParameters[2] = value; simulator.SetFloat("r", simulationParameters[2]); } } // amount of rain per pixel
    public float erosionSpeed { get { return simulationParameters[3]; } set { simulationParameters[3] = value; simulator.SetFloat("e", simulationParameters[3]); } }
    public float erosionExponent { get { return simulationParameters[4]; } set { simulationParameters[4] = value; simulator.SetFloat("m", simulationParameters[4]); } } // water depth multiplier
    public float erosionExponent2 { get { return simulationParameters[5]; } set { simulationParameters[5] = value; simulator.SetFloat("n", simulationParameters[5]); } } // slope multiplier
    public float sedimentationRate { get { return simulationParameters[6]; } set { simulationParameters[6] = value; simulator.SetFloat("s", simulationParameters[6]); } }
    public float timestepLength { get { return simulationParameters[7]; } set { simulationParameters[7] = value; simulator.SetFloat("dt", simulationParameters[7]); } } // timestep

    int steps = 0;

    public TerrainSimulator(ComputeShader simulator)
    {
        this.simulator = simulator;

        if (File.Exists("config/lastSimParams.data")) Load("config/lastSimParams.data");
        else if (File.Exists("config/defaultSimParams.data")) Load("config/defaultSimParams.data");

        uplift = 0;
        rain = 0;
        timestepLength = 0.1f;
        erosionExponent = 0.2f;
        erosionExponent2 = 0.4f;
        sedimentationRate = 5f;
    }

    public void OnDestroy()
    {
        if(texA) texA.Release();
        if(texB) texB.Release();

        if (!Directory.Exists("config")) Directory.CreateDirectory("config");
        Save("config/lastSimParams.data");
    }

    public void GenerateSeedTexture(uint octaves, float cellSize, Vector4 seed)
    {
        int[] dim = { texA.width, texA.height };
        simulator.SetInts("dim", dim);
        simulator.SetFloat("cellSize", cellSize);
        simulator.SetFloat("doubleCellSize", cellSize * 2);

        float left = -((float)texA.width - 1) / 2;
        float bottom = -((float)texA.height - 1) / 2;

        simulator.SetInt("octaves", (int)octaves);
        simulator.SetFloat("noiseScale", 2500 / cellSize);
        simulator.SetFloat("width", texA.width - 1);
        simulator.SetFloat("height", texA.height - 1);
        simulator.SetFloat("left", left);
        simulator.SetFloat("bottom", bottom);

        float[] seedValues = { seed.x, seed.y, seed.z, seed.w };
        simulator.SetFloats("seed", seedValues);

        if (generationKernel == -1)
        {
            generationKernel = simulator.FindKernel("Generate");
        }

        simulator.SetTexture(generationKernel, "genResult", readA ? texA : texB);
        simulator.Dispatch(generationKernel, texA.width / 8, texA.height / 8, 1);

        steps = 0;
        creepSpeed = 0;
        erosionSpeed = 0;
    }

    public void RunSimulationStep(Vector3Int threadGroups)
    {
        if (simulationKernel == -1)
        {
            simulationKernel = simulator.FindKernel("Simulate");
        }

        simulator.SetTexture(simulationKernel, "input", readA ? texA : texB);
        simulator.SetTexture(simulationKernel, "result", readA ? texB : texA);

        simulator.Dispatch(simulationKernel, threadGroups.x, threadGroups.y, threadGroups.z);

        readA = !readA;

        if(steps > 5000)
        {
            creepSpeed = 0.01f;
            erosionSpeed = 0.001f;
        }
    }


    public void Init(Vector2Int gridDimensions)
    {
        if (texA != null)
        {
            texA.Release();
            texA.width = gridDimensions.x;
            texA.height = gridDimensions.y;
        }
        else
        {
            texA = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            texA.enableRandomWrite = true;
        }
        if (texB != null)
        {
            texB.Release();
            texB.width = gridDimensions.x;
            texB.height = gridDimensions.y;
        }
        else
        {
            texB = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            texB.enableRandomWrite = true;
        }

        texA.Create();
        texB.Create();
    }


    public void Load(string filePath)
    {
        byte[] data = File.ReadAllBytes(filePath);
        float[] values = new float[8];
        Buffer.BlockCopy(data, 0, values, 0, values.Length);

        uplift = values[0];
        creepSpeed = values[1];
        rain = values[2];
        erosionSpeed = values[3];
        erosionExponent = values[4];
        erosionExponent2 = values[5];
        sedimentationRate = values[6];
        timestepLength = values[7];
    }

    public void Save(string filePath) 
    {
        byte[] data = new byte[8 * sizeof(float)];
        Buffer.BlockCopy(simulationParameters,0,data,0, data.Length);
        File.WriteAllBytes(filePath, data);
    }
}
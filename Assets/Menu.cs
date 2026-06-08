using UnityEngine;

public class Menu : MonoBehaviour
{
    [SerializeReference] SimulationController simulation;

    // Seed Texture Generation
    public string width
    {
        set
        {
            if (int.TryParse(value, out int widthInt))
            {
                simulation.SetGridDimensions(new Vector2Int(widthInt, widthInt));
            }
        }
    }
    public string height
    {
        set
        {
            if (int.TryParse(value, out int heightInt))
            {
                simulation.SetGridDimensions(new Vector2Int(heightInt, heightInt));
            }
        }
    }
    public string octaves
    {
        set
        {
            if (int.TryParse(value, out int octavesInt))
            {
                simulation.Octaves = (uint)octavesInt;
            }
        }
    }
    public string gridWidth
    {
        set
        {
            if (float.TryParse(value, out float gridWidthFloat))
            {
                simulation.GridWidth = gridWidthFloat;
            }
        }
    }
    public string x
    {
        set
        {
            if (float.TryParse(value, out float xFloat))
            {
                Vector4 seed = simulation.Seed;
                seed.x = xFloat;
                simulation.Seed = seed;
            }
        }
    }
    public string y
    {
        set
        {
            if (float.TryParse(value, out float yFloat))
            {
                Vector4 seed = simulation.Seed;
                seed.y = yFloat;
                simulation.Seed = seed;
            }
        }
    }
    public string z
    {
        set
        {
            if (float.TryParse(value, out float zFloat))
            {
                Vector4 seed = simulation.Seed;
                seed.z = zFloat;
                simulation.Seed = seed;
            }
        }
    }
    public string w
    {
        set
        {
            if (float.TryParse(value, out float wFloat))
            {
                Vector4 seed = simulation.Seed;
                seed.w = wFloat;
                simulation.Seed = seed;
            }
        }
    }

    // Simulation Parameters
    public string duration
    {
        set
        {
            if (int.TryParse(value, out int durationInt))
            {
                simulation.SetTargetSteps(durationInt);
            }
        }
    }
    public string timestepLength
    {
        set
        {
            if (float.TryParse(value, out float timestepLength))
            {
                
            }
        }
    }
}

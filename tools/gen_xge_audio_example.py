"""Generate the deterministic, original 400 Hz/250 ms mono example tone."""
from pathlib import Path
import argparse, struct, wave


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,default=Path(__file__).resolve().parents[1]/'lib/bindings/xge/examples/data/tone.wav')
    a=p.parse_args()
    # Fixed integer samples avoid platform floating-point synthesis differences.
    period=(0,927,1763,2427,2853,3000,2853,2427,1763,927,
            0,-927,-1763,-2427,-2853,-3000,-2853,-2427,-1763,-927)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(a.out),'wb') as f:
        f.setparams((1,2,8000,0,'NONE','not compressed'))
        f.writeframes(struct.pack('<20h',*period)*100)


if __name__=='__main__':
    main()
